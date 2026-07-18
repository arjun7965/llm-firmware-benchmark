#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdalign.h>

#include "dma_cache_transfer.h"
#include "mock_dma_cache.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
              __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

static const void *cache_line_start(const void *address) {
  return (const void *)((uintptr_t)address &
    ~((uintptr_t)DMA_CACHE_LINE_BYTES - 1u));
}

static bool operation_matches(
  size_t index,
  mock_dma_cache_operation_t kind,
  const void *address,
  size_t length
) {
  return mock_dma_cache_operation_kind(index) == kind &&
    mock_dma_cache_operation_address(index) == address &&
    mock_dma_cache_operation_length(index) == length;
}

static bool transfer_state_equals(
  const dma_cache_transfer_t *left,
  const dma_cache_transfer_t *right
) {
  return left->rx_buffer == right->rx_buffer &&
    left->rx_length == right->rx_length &&
    left->initialized == right->initialized &&
    left->rx_in_flight == right->rx_in_flight;
}

static bool test_initialization_and_invalid_calls(void) {
  alignas(DMA_CACHE_LINE_BYTES) uint8_t storage[128u] = { 0 };
  dma_cache_transfer_t transfer = {
    .rx_buffer = storage,
    .rx_length = 7u,
    .initialized = false,
    .rx_in_flight = true,
  };
  const dma_cache_transfer_t before = transfer;

  mock_dma_cache_reset();
  CHECK(!dma_cache_transfer_init(NULL));
  CHECK(transfer_state_equals(&transfer, &before));
  CHECK(
    dma_cache_transfer_start_tx(&transfer, storage, 4u) ==
      DMA_CACHE_STATUS_INVALID_ARGUMENT
  );
  CHECK(
    dma_cache_transfer_start_rx(&transfer, storage, 4u) ==
      DMA_CACHE_STATUS_INVALID_ARGUMENT
  );
  CHECK(
    dma_cache_transfer_finish_rx(&transfer) == DMA_CACHE_STATUS_INVALID_ARGUMENT
  );
  CHECK(mock_dma_cache_operation_count() == 0u);

  CHECK(dma_cache_transfer_init(&transfer));
  CHECK(transfer.initialized);
  CHECK(!transfer.rx_in_flight);
  CHECK(transfer.rx_buffer == NULL);
  CHECK(transfer.rx_length == 0u);
  CHECK(
    dma_cache_transfer_start_tx(&transfer, NULL, 4u) ==
      DMA_CACHE_STATUS_INVALID_ARGUMENT
  );
  CHECK(
    dma_cache_transfer_start_tx(&transfer, storage, 0u) ==
      DMA_CACHE_STATUS_INVALID_ARGUMENT
  );
  CHECK(
    dma_cache_transfer_start_tx(
      &transfer,
      storage,
      DMA_MAX_TRANSFER_BYTES + 1u
    ) == DMA_CACHE_STATUS_INVALID_ARGUMENT
  );
  CHECK(
    dma_cache_transfer_start_rx(&transfer, storage + 1u, 4u) ==
      DMA_CACHE_STATUS_INVALID_ARGUMENT
  );
  CHECK(mock_dma_cache_operation_count() == 0u);
  return true;
}

static bool test_tx_clean_order_and_cache_line_rounding(void) {
  alignas(DMA_CACHE_LINE_BYTES) uint8_t storage[128u] = { 0 };
  dma_cache_transfer_t transfer = { 0 };
  const uint8_t *single_line = storage + 4u;
  const uint8_t *crossing = storage + 28u;

  CHECK(dma_cache_transfer_init(&transfer));
  mock_dma_cache_reset();
  CHECK(
    dma_cache_transfer_start_tx(&transfer, single_line, 12u) ==
      DMA_CACHE_STATUS_OK
  );
  CHECK(mock_dma_cache_operation_count() == 2u);
  CHECK(operation_matches(
    0u,
    MOCK_DMA_CACHE_OPERATION_CLEAN,
    cache_line_start(single_line),
    DMA_CACHE_LINE_BYTES
  ));
  CHECK(operation_matches(
    1u,
    MOCK_DMA_CACHE_OPERATION_START_TX,
    single_line,
    12u
  ));

  mock_dma_cache_reset();
  mock_dma_cache_force_next_tx_status(DMA_CACHE_STATUS_ERROR);
  CHECK(
    dma_cache_transfer_start_tx(&transfer, crossing, 8u) ==
      DMA_CACHE_STATUS_ERROR
  );
  CHECK(mock_dma_cache_operation_count() == 2u);
  CHECK(operation_matches(
    0u,
    MOCK_DMA_CACHE_OPERATION_CLEAN,
    cache_line_start(crossing),
    DMA_CACHE_LINE_BYTES * 2u
  ));
  CHECK(operation_matches(
    1u,
    MOCK_DMA_CACHE_OPERATION_START_TX,
    crossing,
    8u
  ));
  return true;
}

static bool test_rx_invalidation_completion_and_busy_retry(void) {
  alignas(DMA_CACHE_LINE_BYTES) uint8_t storage[128u] = { 0 };
  dma_cache_transfer_t transfer = { 0 };
  uint8_t *buffer = storage + 4u;

  CHECK(dma_cache_transfer_init(&transfer));
  mock_dma_cache_reset();
  CHECK(
    dma_cache_transfer_start_rx(&transfer, buffer, 12u) ==
      DMA_CACHE_STATUS_OK
  );
  CHECK(transfer.rx_in_flight);
  CHECK(transfer.rx_buffer == buffer);
  CHECK(transfer.rx_length == 12u);
  CHECK(mock_dma_cache_operation_count() == 2u);
  CHECK(operation_matches(
    0u,
    MOCK_DMA_CACHE_OPERATION_INVALIDATE,
    cache_line_start(buffer),
    DMA_CACHE_LINE_BYTES
  ));
  CHECK(operation_matches(
    1u,
    MOCK_DMA_CACHE_OPERATION_START_RX,
    buffer,
    12u
  ));
  CHECK(
    dma_cache_transfer_start_rx(&transfer, buffer, 12u) ==
      DMA_CACHE_STATUS_INVALID_ARGUMENT
  );
  CHECK(mock_dma_cache_operation_count() == 2u);

  mock_dma_cache_force_next_rx_finish_status(DMA_CACHE_STATUS_BUSY);
  CHECK(
    dma_cache_transfer_finish_rx(&transfer) == DMA_CACHE_STATUS_BUSY
  );
  CHECK(transfer.rx_in_flight);
  CHECK(mock_dma_cache_operation_count() == 3u);
  CHECK(operation_matches(
    2u,
    MOCK_DMA_CACHE_OPERATION_FINISH_RX,
    NULL,
    0u
  ));

  CHECK(dma_cache_transfer_finish_rx(&transfer) == DMA_CACHE_STATUS_OK);
  CHECK(!transfer.rx_in_flight);
  CHECK(transfer.rx_buffer == NULL);
  CHECK(transfer.rx_length == 0u);
  CHECK(mock_dma_cache_operation_count() == 5u);
  CHECK(operation_matches(
    3u,
    MOCK_DMA_CACHE_OPERATION_FINISH_RX,
    NULL,
    0u
  ));
  CHECK(operation_matches(
    4u,
    MOCK_DMA_CACHE_OPERATION_INVALIDATE,
    cache_line_start(buffer),
    DMA_CACHE_LINE_BYTES
  ));
  return true;
}

static bool test_rx_failure_does_not_publish_or_invalidate_after_error(void) {
  alignas(DMA_CACHE_LINE_BYTES) uint8_t storage[128u] = { 0 };
  dma_cache_transfer_t transfer = { 0 };
  uint8_t *buffer = storage + 28u;

  CHECK(dma_cache_transfer_init(&transfer));
  mock_dma_cache_reset();
  mock_dma_cache_force_next_rx_start_status(DMA_CACHE_STATUS_ERROR);
  CHECK(
    dma_cache_transfer_start_rx(&transfer, buffer, 8u) ==
      DMA_CACHE_STATUS_ERROR
  );
  CHECK(!transfer.rx_in_flight);
  CHECK(mock_dma_cache_operation_count() == 2u);
  CHECK(operation_matches(
    0u,
    MOCK_DMA_CACHE_OPERATION_INVALIDATE,
    cache_line_start(buffer),
    DMA_CACHE_LINE_BYTES * 2u
  ));
  CHECK(operation_matches(
    1u,
    MOCK_DMA_CACHE_OPERATION_START_RX,
    buffer,
    8u
  ));

  mock_dma_cache_reset();
  CHECK(
    dma_cache_transfer_start_rx(&transfer, buffer, 8u) ==
      DMA_CACHE_STATUS_OK
  );
  mock_dma_cache_force_next_rx_finish_status(DMA_CACHE_STATUS_ERROR);
  CHECK(
    dma_cache_transfer_finish_rx(&transfer) == DMA_CACHE_STATUS_ERROR
  );
  CHECK(!transfer.rx_in_flight);
  CHECK(transfer.rx_buffer == NULL);
  CHECK(transfer.rx_length == 0u);
  CHECK(mock_dma_cache_operation_count() == 3u);
  CHECK(operation_matches(
    2u,
    MOCK_DMA_CACHE_OPERATION_FINISH_RX,
    NULL,
    0u
  ));
  CHECK(
    dma_cache_transfer_finish_rx(&transfer) == DMA_CACHE_STATUS_INVALID_ARGUMENT
  );
  CHECK(mock_dma_cache_operation_count() == 3u);
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "initialization and invalid calls", test_initialization_and_invalid_calls },
    { "tx clean order and cache rounding", test_tx_clean_order_and_cache_line_rounding },
    { "rx invalidation, completion, and busy retry", test_rx_invalidation_completion_and_busy_retry },
    { "rx failure cleanup", test_rx_failure_does_not_publish_or_invalidate_after_error },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) return 1;
    printf("ok - %s\n", tests[index].name);
  }
  return 0;
}

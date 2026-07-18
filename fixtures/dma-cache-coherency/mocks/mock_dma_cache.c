#include "mock_dma_cache.h"

#define MOCK_DMA_CACHE_MAX_OPERATIONS 16u

typedef struct {
  mock_dma_cache_operation_t kind;
  const void *address;
  size_t length;
} mock_dma_cache_operation_record_t;

static mock_dma_cache_operation_record_t operations[MOCK_DMA_CACHE_MAX_OPERATIONS];
static size_t operation_count;
static dma_cache_status_t next_tx_status;
static dma_cache_status_t next_rx_start_status;
static dma_cache_status_t next_rx_finish_status;

static void record_operation(
  mock_dma_cache_operation_t kind,
  const void *address,
  size_t length
) {
  if (operation_count >= MOCK_DMA_CACHE_MAX_OPERATIONS) return;
  operations[operation_count++] = (mock_dma_cache_operation_record_t) {
    .kind = kind,
    .address = address,
    .length = length,
  };
}

void mock_dma_cache_reset(void) {
  operation_count = 0u;
  next_tx_status = DMA_CACHE_STATUS_OK;
  next_rx_start_status = DMA_CACHE_STATUS_OK;
  next_rx_finish_status = DMA_CACHE_STATUS_OK;
}

void mock_dma_cache_force_next_tx_status(dma_cache_status_t status) {
  next_tx_status = status;
}

void mock_dma_cache_force_next_rx_start_status(dma_cache_status_t status) {
  next_rx_start_status = status;
}

void mock_dma_cache_force_next_rx_finish_status(dma_cache_status_t status) {
  next_rx_finish_status = status;
}

size_t mock_dma_cache_operation_count(void) {
  return operation_count;
}

mock_dma_cache_operation_t mock_dma_cache_operation_kind(size_t index) {
  if (index >= operation_count) return MOCK_DMA_CACHE_OPERATION_CLEAN;
  return operations[index].kind;
}

const void *mock_dma_cache_operation_address(size_t index) {
  if (index >= operation_count) return NULL;
  return operations[index].address;
}

size_t mock_dma_cache_operation_length(size_t index) {
  if (index >= operation_count) return 0u;
  return operations[index].length;
}

void dma_cache_clean_by_address(const void *address, size_t length) {
  record_operation(MOCK_DMA_CACHE_OPERATION_CLEAN, address, length);
}

void dma_cache_invalidate_by_address(void *address, size_t length) {
  record_operation(MOCK_DMA_CACHE_OPERATION_INVALIDATE, address, length);
}

dma_cache_status_t dma_cache_start_tx(const uint8_t *buffer, size_t length) {
  const dma_cache_status_t status = next_tx_status;

  record_operation(MOCK_DMA_CACHE_OPERATION_START_TX, buffer, length);
  next_tx_status = DMA_CACHE_STATUS_OK;
  return status;
}

dma_cache_status_t dma_cache_start_rx(uint8_t *buffer, size_t length) {
  const dma_cache_status_t status = next_rx_start_status;

  record_operation(MOCK_DMA_CACHE_OPERATION_START_RX, buffer, length);
  next_rx_start_status = DMA_CACHE_STATUS_OK;
  return status;
}

dma_cache_status_t dma_cache_finish_rx(void) {
  const dma_cache_status_t status = next_rx_finish_status;

  record_operation(MOCK_DMA_CACHE_OPERATION_FINISH_RX, NULL, 0u);
  next_rx_finish_status = DMA_CACHE_STATUS_OK;
  return status;
}

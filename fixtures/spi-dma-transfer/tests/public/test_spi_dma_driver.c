#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "mock_spi_dma.h"
#include "spi_dma_driver.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
              __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

static uint32_t idle_control(void) {
  return SPI0_CONTROL_ENABLE | SPI0_CONTROL_MASTER;
}

static uint32_t tx_control(void) {
  return DMA0_CHANNEL_CONTROL_ENABLE |
    DMA0_CHANNEL_CONTROL_SOURCE_INCREMENT |
    DMA0_CHANNEL_CONTROL_COMPLETE_IRQ_ENABLE |
    DMA0_CHANNEL_CONTROL_ERROR_IRQ_ENABLE;
}

static uint32_t rx_control(void) {
  return DMA0_CHANNEL_CONTROL_ENABLE |
    DMA0_CHANNEL_CONTROL_DESTINATION_INCREMENT |
    DMA0_CHANNEL_CONTROL_COMPLETE_IRQ_ENABLE |
    DMA0_CHANNEL_CONTROL_ERROR_IRQ_ENABLE;
}

static bool initialize(spi_dma_driver_t *driver) {
  return spi_dma_init(driver, mock_spi0(), mock_dma0());
}

static bool events_match_from(
  size_t offset,
  const mock_spi_dma_event_t *expected,
  size_t expected_count
) {
  if (mock_spi_dma_event_count() != offset + expected_count) return false;
  for (size_t index = 0u; index < expected_count; index++) {
    if (mock_spi_dma_event_at(offset + index) != expected[index]) {
      return false;
    }
  }
  return true;
}

static bool test_initialization_sequence_and_recovery(void) {
  spi_dma_driver_t driver = {
    .tx_buffer = (const uint8_t *)(uintptr_t)UINT32_C(1),
    .rx_buffer = (uint8_t *)(uintptr_t)UINT32_C(1),
    .transfer_length = 7u,
    .error_flags = DMA0_STATUS_ERROR_MASK,
    .result = SPI_DMA_RESULT_ERROR,
    .tx_complete = true,
    .rx_complete = true,
    .busy = true,
    .initialized = true,
  };
  const uint8_t tx[] = { UINT8_C(0x10) };
  uint8_t rx[sizeof(tx)] = { 0 };
  const mock_spi_dma_event_t initialization_events[] = {
    MOCK_SPI_DMA_EVENT_SPI_CONTROL,
    MOCK_SPI_DMA_EVENT_DMA_TX_CONTROL,
    MOCK_SPI_DMA_EVENT_DMA_RX_CONTROL,
    MOCK_SPI_DMA_EVENT_DMA_STATUS_CLEAR,
    MOCK_SPI_DMA_EVENT_SPI_CLOCK_DIVIDER,
    MOCK_SPI_DMA_EVENT_SPI_CHIP_SELECT,
    MOCK_SPI_DMA_EVENT_SPI_CONTROL,
  };

  mock_spi_dma_reset();
  CHECK(!spi_dma_init(NULL, mock_spi0(), mock_dma0()));
  CHECK(!spi_dma_init(&driver, NULL, mock_dma0()));
  CHECK(!spi_dma_init(&driver, mock_spi0(), NULL));
  CHECK(mock_spi_control_write_count() == 0u);
  CHECK(mock_dma_tx_control_write_count() == 0u);
  CHECK(mock_dma_rx_control_write_count() == 0u);
  CHECK(mock_dma_status_clear_write_count() == 0u);
  CHECK(mock_spi_dma_irq_save_count() == 0u);

  CHECK(initialize(&driver));
  CHECK(driver.spi == mock_spi0());
  CHECK(driver.dma == mock_dma0());
  CHECK(driver.tx_buffer == NULL);
  CHECK(driver.rx_buffer == NULL);
  CHECK(driver.transfer_length == 0u);
  CHECK(driver.error_flags == 0u);
  CHECK(driver.result == SPI_DMA_RESULT_NONE);
  CHECK(!driver.tx_complete);
  CHECK(!driver.rx_complete);
  CHECK(!driver.busy);
  CHECK(driver.initialized);
  CHECK(mock_spi_control_write_count() == 2u);
  CHECK(mock_spi_control_at(0u) == 0u);
  CHECK(mock_spi_control_at(1u) == idle_control());
  CHECK(mock_spi_clock_divider_write_count() == 1u);
  CHECK(mock_spi_clock_divider() == SPI_DMA_CLOCK_DIVIDER);
  CHECK(mock_spi_chip_select_write_count() == 1u);
  CHECK(mock_spi_chip_select() == SPI0_CHIP_SELECT_DEASSERTED);
  CHECK(mock_dma_tx_control_write_count() == 1u);
  CHECK(mock_dma_tx_control() == 0u);
  CHECK(mock_dma_rx_control_write_count() == 1u);
  CHECK(mock_dma_rx_control() == 0u);
  CHECK(mock_dma_status_clear_write_count() == 1u);
  CHECK(mock_dma_last_status_clear() == DMA0_STATUS_ALL);
  CHECK(events_match_from(
    0u,
    initialization_events,
    sizeof(initialization_events) / sizeof(initialization_events[0])
  ));
  CHECK(mock_spi_dma_irq_save_count() == 0u);

  CHECK(spi_dma_start(&driver, tx, rx, sizeof(tx)));
  CHECK(driver.busy);
  CHECK(initialize(&driver));
  CHECK(!driver.busy);
  CHECK(driver.result == SPI_DMA_RESULT_NONE);
  CHECK(driver.tx_buffer == NULL);
  CHECK(driver.rx_buffer == NULL);
  CHECK(mock_spi_control() == idle_control());
  CHECK(mock_spi_chip_select() == SPI0_CHIP_SELECT_DEASSERTED);
  CHECK(mock_dma_tx_control() == 0u);
  CHECK(mock_dma_rx_control() == 0u);
  return true;
}

static bool test_start_configuration_capacity_and_busy_rejection(void) {
  uint8_t tx[SPI_DMA_MAX_TRANSFER];
  uint8_t rx[SPI_DMA_MAX_TRANSFER] = { 0 };
  spi_dma_driver_t driver = { 0 };
  spi_dma_driver_t uninitialized = { 0 };
  size_t saves;
  size_t restores;
  size_t control_writes;
  size_t status_clears;
  size_t start_event_offset;
  const mock_spi_dma_event_t start_events[] = {
    MOCK_SPI_DMA_EVENT_DMA_STATUS_CLEAR,
    MOCK_SPI_DMA_EVENT_SPI_CHIP_SELECT,
    MOCK_SPI_DMA_EVENT_DMA_RX_SOURCE,
    MOCK_SPI_DMA_EVENT_DMA_RX_DESTINATION,
    MOCK_SPI_DMA_EVENT_DMA_RX_COUNT,
    MOCK_SPI_DMA_EVENT_DMA_RX_CONTROL,
    MOCK_SPI_DMA_EVENT_DMA_TX_SOURCE,
    MOCK_SPI_DMA_EVENT_DMA_TX_DESTINATION,
    MOCK_SPI_DMA_EVENT_DMA_TX_COUNT,
    MOCK_SPI_DMA_EVENT_DMA_TX_CONTROL,
    MOCK_SPI_DMA_EVENT_SPI_CONTROL,
  };

  for (size_t index = 0u; index < sizeof(tx); index++) {
    tx[index] = (uint8_t)(UINT8_C(0x20) + index);
  }
  mock_spi_dma_reset();
  CHECK(!spi_dma_start(&uninitialized, tx, rx, 1u));
  CHECK(mock_spi_dma_irq_save_count() == 0u);
  CHECK(initialize(&driver));

  saves = mock_spi_dma_irq_save_count();
  restores = mock_spi_dma_irq_restore_count();
  CHECK(!spi_dma_start(&driver, NULL, rx, 1u));
  CHECK(!spi_dma_start(&driver, tx, NULL, 1u));
  CHECK(!spi_dma_start(&driver, tx, rx, 0u));
  CHECK(!spi_dma_start(&driver, tx, rx, sizeof(tx) + 1u));
  CHECK(mock_spi_dma_irq_save_count() == saves);
  CHECK(mock_spi_dma_irq_restore_count() == restores);

  mock_spi_dma_set_status(DMA0_STATUS_ALL);
  mock_spi_dma_set_interrupts_enabled(false);
  start_event_offset = mock_spi_dma_event_count();
  CHECK(spi_dma_start(&driver, tx, rx, sizeof(tx)));
  CHECK(driver.busy);
  CHECK(driver.tx_buffer == tx);
  CHECK(driver.rx_buffer == rx);
  CHECK(driver.transfer_length == sizeof(tx));
  CHECK(mock_dma_status() == 0u);
  CHECK(mock_spi_dma_irq_save_count() == saves + 1u);
  CHECK(mock_spi_dma_irq_restore_count() == restores + 1u);
  CHECK(mock_spi_dma_last_irq_restore() == 0u);
  CHECK(!mock_spi_dma_interrupts_enabled());
  CHECK(mock_spi_chip_select() == SPI0_CHIP_SELECT_ASSERTED);
  CHECK(mock_spi_control() == (idle_control() | SPI0_CONTROL_DMA_MASK));
  CHECK(mock_dma_rx_source() == mock_spi_data_address());
  CHECK(mock_dma_rx_destination() == spi_dma_buffer_address(rx));
  CHECK(mock_dma_rx_count() == sizeof(rx));
  CHECK(mock_dma_rx_control() == rx_control());
  CHECK(mock_dma_tx_source() == spi_dma_buffer_address(tx));
  CHECK(mock_dma_tx_destination() == mock_spi_data_address());
  CHECK(mock_dma_tx_count() == sizeof(tx));
  CHECK(mock_dma_tx_control() == tx_control());
  CHECK(mock_dma_status_clear_write_count() == 2u);
  CHECK(mock_dma_last_status_clear() == DMA0_STATUS_ALL);
  CHECK(events_match_from(
    start_event_offset,
    start_events,
    sizeof(start_events) / sizeof(start_events[0])
  ));

  control_writes = mock_spi_control_write_count();
  status_clears = mock_dma_status_clear_write_count();
  saves = mock_spi_dma_irq_save_count();
  restores = mock_spi_dma_irq_restore_count();
  CHECK(!spi_dma_start(&driver, tx, rx, 1u));
  CHECK(mock_spi_control_write_count() == control_writes);
  CHECK(mock_dma_status_clear_write_count() == status_clears);
  CHECK(mock_spi_dma_irq_save_count() == saves + 1u);
  CHECK(mock_spi_dma_irq_restore_count() == restores + 1u);
  CHECK(!mock_spi_dma_interrupts_enabled());
  return true;
}

static bool test_completion_requires_both_channels_and_preserves_buffers(void) {
  const uint8_t tx[] = {
    UINT8_C(0x01), UINT8_C(0x23), UINT8_C(0x45), UINT8_C(0x67),
  };
  uint8_t rx[sizeof(tx)] = { 0 };
  spi_dma_driver_t driver = { 0 };
  uint32_t error_flags = UINT32_MAX;
  size_t saves;
  size_t restores;
  size_t finish_event_offset;
  const mock_spi_dma_event_t finish_events[] = {
    MOCK_SPI_DMA_EVENT_DMA_STATUS_CLEAR,
    MOCK_SPI_DMA_EVENT_SPI_CONTROL,
    MOCK_SPI_DMA_EVENT_DMA_TX_CONTROL,
    MOCK_SPI_DMA_EVENT_DMA_RX_CONTROL,
    MOCK_SPI_DMA_EVENT_SPI_CHIP_SELECT,
  };

  mock_spi_dma_reset();
  CHECK(initialize(&driver));
  CHECK(spi_dma_start(&driver, tx, rx, sizeof(tx)));
  saves = mock_spi_dma_irq_save_count();
  restores = mock_spi_dma_irq_restore_count();

  CHECK(mock_spi_dma_signal_tx_complete());
  spi_dma_irq(&driver);
  CHECK(driver.busy);
  CHECK(driver.tx_complete);
  CHECK(!driver.rx_complete);
  CHECK(driver.result == SPI_DMA_RESULT_NONE);
  CHECK(mock_dma_status_read_count() == 1u);
  CHECK(mock_dma_status_clear_write_count() == 3u);
  CHECK(mock_dma_last_status_clear() == DMA0_STATUS_TX_COMPLETE);
  CHECK(mock_dma_tx_control() == tx_control());
  CHECK(mock_dma_rx_control() == rx_control());
  CHECK(mock_spi_dma_irq_save_count() == saves);
  CHECK(mock_spi_dma_irq_restore_count() == restores);

  CHECK(mock_spi_dma_signal_rx_complete());
  finish_event_offset = mock_spi_dma_event_count();
  spi_dma_irq(&driver);
  CHECK(!driver.busy);
  CHECK(driver.result == SPI_DMA_RESULT_COMPLETE);
  CHECK(driver.tx_buffer == NULL);
  CHECK(driver.rx_buffer == NULL);
  CHECK(driver.transfer_length == 0u);
  CHECK(mock_dma_status_read_count() == 2u);
  CHECK(mock_dma_status_clear_write_count() == 4u);
  CHECK(mock_dma_last_status_clear() == DMA0_STATUS_RX_COMPLETE);
  CHECK(mock_dma_tx_control() == 0u);
  CHECK(mock_dma_rx_control() == 0u);
  CHECK(mock_spi_control() == idle_control());
  CHECK(mock_spi_chip_select() == SPI0_CHIP_SELECT_DEASSERTED);
  CHECK(events_match_from(
    finish_event_offset,
    finish_events,
    sizeof(finish_events) / sizeof(finish_events[0])
  ));
  for (size_t index = 0u; index < sizeof(tx); index++) {
    CHECK(rx[index] == (uint8_t)(tx[index] ^ MOCK_SPI_DMA_RESPONSE_XOR));
  }

  mock_spi_dma_set_interrupts_enabled(false);
  CHECK(!spi_dma_is_busy(&driver));
  CHECK(!mock_spi_dma_interrupts_enabled());
  CHECK(spi_dma_take_result(&driver, &error_flags) ==
    SPI_DMA_RESULT_COMPLETE);
  CHECK(error_flags == 0u);
  CHECK(driver.result == SPI_DMA_RESULT_NONE);
  CHECK(!mock_spi_dma_interrupts_enabled());
  CHECK(mock_spi_dma_last_irq_restore() == 0u);
  CHECK(spi_dma_take_result(&driver, &error_flags) ==
    SPI_DMA_RESULT_NONE);
  CHECK(error_flags == 0u);
  return true;
}

static bool test_error_teardown_priority_and_recovery(void) {
  const uint8_t tx[] = { UINT8_C(0x9A), UINT8_C(0xBC) };
  uint8_t rx[sizeof(tx)] = { 0 };
  spi_dma_driver_t driver = { 0 };
  uint32_t error_flags = 0u;

  mock_spi_dma_reset();
  CHECK(initialize(&driver));
  CHECK(spi_dma_start(&driver, tx, rx, sizeof(tx)));
  mock_spi_dma_set_status(
    DMA0_STATUS_TX_COMPLETE | DMA0_STATUS_RX_COMPLETE |
      DMA0_STATUS_RX_ERROR
  );
  spi_dma_irq(&driver);
  CHECK(!driver.busy);
  CHECK(!driver.tx_complete);
  CHECK(!driver.rx_complete);
  CHECK(driver.result == SPI_DMA_RESULT_ERROR);
  CHECK(driver.error_flags == DMA0_STATUS_RX_ERROR);
  CHECK(mock_dma_last_status_clear() ==
    (DMA0_STATUS_TX_COMPLETE | DMA0_STATUS_RX_COMPLETE |
      DMA0_STATUS_RX_ERROR));
  CHECK(mock_dma_tx_control() == 0u);
  CHECK(mock_dma_rx_control() == 0u);
  CHECK(mock_spi_control() == idle_control());
  CHECK(mock_spi_chip_select() == SPI0_CHIP_SELECT_DEASSERTED);
  CHECK(!spi_dma_start(&driver, tx, rx, sizeof(tx)));

  CHECK(spi_dma_take_result(&driver, &error_flags) == SPI_DMA_RESULT_ERROR);
  CHECK(error_flags == DMA0_STATUS_RX_ERROR);
  CHECK(driver.error_flags == 0u);
  CHECK(driver.result == SPI_DMA_RESULT_NONE);

  CHECK(spi_dma_start(&driver, tx, rx, sizeof(tx)));
  CHECK(mock_spi_dma_signal_tx_complete());
  spi_dma_irq(&driver);
  CHECK(mock_spi_dma_signal_rx_complete());
  spi_dma_irq(&driver);
  CHECK(spi_dma_take_result(&driver, &error_flags) ==
    SPI_DMA_RESULT_COMPLETE);
  CHECK(error_flags == 0u);
  return true;
}

static bool test_irq_bounds_stale_status_and_invalid_calls(void) {
  const uint8_t tx[] = { UINT8_C(0x55) };
  uint8_t rx[sizeof(tx)] = { 0 };
  spi_dma_driver_t driver = { 0 };
  spi_dma_driver_t uninitialized = { 0 };
  uint32_t error_flags = UINT32_MAX;
  size_t saves;
  size_t restores;
  size_t status_reads;
  size_t status_clears;

  mock_spi_dma_reset();
  CHECK(!spi_dma_is_busy(NULL));
  CHECK(!spi_dma_is_busy(&uninitialized));
  CHECK(spi_dma_take_result(NULL, &error_flags) == SPI_DMA_RESULT_NONE);
  CHECK(spi_dma_take_result(&uninitialized, &error_flags) ==
    SPI_DMA_RESULT_NONE);
  spi_dma_irq(NULL);
  spi_dma_irq(&uninitialized);
  CHECK(mock_spi_dma_irq_save_count() == 0u);
  CHECK(mock_dma_status_read_count() == 0u);

  CHECK(initialize(&driver));
  saves = mock_spi_dma_irq_save_count();
  restores = mock_spi_dma_irq_restore_count();
  CHECK(spi_dma_take_result(&driver, NULL) == SPI_DMA_RESULT_NONE);
  CHECK(mock_spi_dma_irq_save_count() == saves);
  CHECK(mock_spi_dma_irq_restore_count() == restores);

  mock_spi_dma_set_status(DMA0_STATUS_COMPLETE_MASK);
  CHECK(spi_dma_start(&driver, tx, rx, sizeof(tx)));
  CHECK(mock_dma_status() == 0u);
  mock_spi_dma_set_interrupts_enabled(false);
  CHECK(spi_dma_is_busy(&driver));
  CHECK(!mock_spi_dma_interrupts_enabled());
  CHECK(spi_dma_take_result(&driver, &error_flags) ==
    SPI_DMA_RESULT_NONE);
  CHECK(error_flags == 0u);
  CHECK(!mock_spi_dma_interrupts_enabled());
  CHECK(mock_spi_dma_last_irq_restore() == 0u);

  status_reads = mock_dma_status_read_count();
  status_clears = mock_dma_status_clear_write_count();
  saves = mock_spi_dma_irq_save_count();
  restores = mock_spi_dma_irq_restore_count();
  spi_dma_irq(&driver);
  CHECK(mock_dma_status_read_count() == status_reads + 1u);
  CHECK(mock_dma_status_clear_write_count() == status_clears);
  CHECK(mock_spi_dma_irq_save_count() == saves);
  CHECK(mock_spi_dma_irq_restore_count() == restores);
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "initialization sequence and recovery", test_initialization_sequence_and_recovery },
    { "start configuration, capacity, and busy rejection", test_start_configuration_capacity_and_busy_rejection },
    { "completion requires both channels", test_completion_requires_both_channels_and_preserves_buffers },
    { "error teardown priority and recovery", test_error_teardown_priority_and_recovery },
    { "IRQ bounds, stale status, and invalid calls", test_irq_bounds_stale_status_and_invalid_calls },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) return 1;
    printf("ok - %s\n", tests[index].name);
  }
  return 0;
}

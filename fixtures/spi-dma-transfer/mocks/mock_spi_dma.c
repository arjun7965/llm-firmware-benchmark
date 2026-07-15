#include "mock_spi_dma.h"

#define MOCK_SPI_DMA_HISTORY_CAPACITY 64u

struct spi0_registers {
  uint32_t control;
  uint32_t clock_divider;
  uint32_t chip_select;
  uint32_t data;
};

struct dma0_registers {
  uintptr_t tx_source;
  uintptr_t tx_destination;
  uint32_t tx_count;
  uint32_t tx_control;
  uintptr_t rx_source;
  uintptr_t rx_destination;
  uint32_t rx_count;
  uint32_t rx_control;
  uint32_t status;
  uint32_t status_clear;
};

typedef struct {
  struct spi0_registers spi;
  struct dma0_registers dma;
  mock_spi_dma_event_t event_history[MOCK_SPI_DMA_HISTORY_CAPACITY];
  uint32_t spi_control_history[MOCK_SPI_DMA_HISTORY_CAPACITY];
  uint32_t spi_chip_select_history[MOCK_SPI_DMA_HISTORY_CAPACITY];
  size_t event_count;
  size_t spi_control_count;
  size_t spi_clock_divider_count;
  size_t spi_chip_select_count;
  size_t dma_tx_control_count;
  size_t dma_rx_control_count;
  size_t dma_status_read_count;
  size_t dma_status_clear_count;
  uint32_t last_dma_status_clear;
  bool interrupts_enabled;
  size_t irq_save_count;
  size_t irq_restore_count;
  uint32_t last_irq_restore;
} mock_spi_dma_state_t;

static mock_spi_dma_state_t state;

static bool is_spi(const volatile spi0_registers_t *spi) {
  return spi == &state.spi;
}

static bool is_dma(const volatile dma0_registers_t *dma) {
  return dma == &state.dma;
}

static void record_event(mock_spi_dma_event_t event) {
  if (state.event_count < MOCK_SPI_DMA_HISTORY_CAPACITY) {
    state.event_history[state.event_count] = event;
  }
  state.event_count++;
}

static bool can_signal_channel(
  uint32_t spi_dma_enable,
  uint32_t channel_control
) {
  const uint32_t required_spi_control = SPI0_CONTROL_ENABLE |
    SPI0_CONTROL_MASTER | spi_dma_enable;
  return (state.spi.control & required_spi_control) ==
      required_spi_control &&
    state.spi.chip_select == SPI0_CHIP_SELECT_ASSERTED &&
    (channel_control & DMA0_CHANNEL_CONTROL_ENABLE) != 0u;
}

static void copy_full_duplex_response(void) {
  if (
    state.dma.tx_source == 0u || state.dma.rx_destination == 0u ||
    state.dma.tx_destination != (uintptr_t)&state.spi.data ||
    state.dma.rx_source != (uintptr_t)&state.spi.data ||
    state.dma.tx_count != state.dma.rx_count
  ) {
    return;
  }

  const uint8_t *tx = (const uint8_t *)state.dma.tx_source;
  uint8_t *rx = (uint8_t *)state.dma.rx_destination;
  for (size_t index = 0u; index < state.dma.tx_count; index++) {
    rx[index] = (uint8_t)(tx[index] ^ MOCK_SPI_DMA_RESPONSE_XOR);
  }
}

void mock_spi_dma_reset(void) {
  state = (mock_spi_dma_state_t){ 0 };
  state.interrupts_enabled = true;
}

volatile spi0_registers_t *mock_spi0(void) {
  return &state.spi;
}

volatile dma0_registers_t *mock_dma0(void) {
  return &state.dma;
}

bool mock_spi_dma_signal_tx_complete(void) {
  if (!can_signal_channel(
    SPI0_CONTROL_TX_DMA_ENABLE,
    state.dma.tx_control
  )) {
    return false;
  }
  state.dma.status |= DMA0_STATUS_TX_COMPLETE;
  return true;
}

bool mock_spi_dma_signal_rx_complete(void) {
  if (!can_signal_channel(
    SPI0_CONTROL_RX_DMA_ENABLE,
    state.dma.rx_control
  )) {
    return false;
  }
  copy_full_duplex_response();
  state.dma.status |= DMA0_STATUS_RX_COMPLETE;
  return true;
}

void mock_spi_dma_set_status(uint32_t status) {
  state.dma.status = status & DMA0_STATUS_ALL;
}

size_t mock_spi_dma_event_count(void) {
  return state.event_count;
}

mock_spi_dma_event_t mock_spi_dma_event_at(size_t index) {
  return index < state.event_count
    ? state.event_history[index]
    : MOCK_SPI_DMA_EVENT_COUNT;
}

uint32_t mock_spi_control(void) {
  return state.spi.control;
}

uint32_t mock_spi_control_at(size_t index) {
  return index < state.spi_control_count
    ? state.spi_control_history[index]
    : 0u;
}

size_t mock_spi_control_write_count(void) {
  return state.spi_control_count;
}

uint32_t mock_spi_clock_divider(void) {
  return state.spi.clock_divider;
}

size_t mock_spi_clock_divider_write_count(void) {
  return state.spi_clock_divider_count;
}

uint32_t mock_spi_chip_select(void) {
  return state.spi.chip_select;
}

uint32_t mock_spi_chip_select_at(size_t index) {
  return index < state.spi_chip_select_count
    ? state.spi_chip_select_history[index]
    : 0u;
}

size_t mock_spi_chip_select_write_count(void) {
  return state.spi_chip_select_count;
}

uintptr_t mock_spi_data_address(void) {
  return (uintptr_t)&state.spi.data;
}

uintptr_t mock_dma_tx_source(void) {
  return state.dma.tx_source;
}

uintptr_t mock_dma_tx_destination(void) {
  return state.dma.tx_destination;
}

uint32_t mock_dma_tx_count(void) {
  return state.dma.tx_count;
}

uint32_t mock_dma_tx_control(void) {
  return state.dma.tx_control;
}

size_t mock_dma_tx_control_write_count(void) {
  return state.dma_tx_control_count;
}

uintptr_t mock_dma_rx_source(void) {
  return state.dma.rx_source;
}

uintptr_t mock_dma_rx_destination(void) {
  return state.dma.rx_destination;
}

uint32_t mock_dma_rx_count(void) {
  return state.dma.rx_count;
}

uint32_t mock_dma_rx_control(void) {
  return state.dma.rx_control;
}

size_t mock_dma_rx_control_write_count(void) {
  return state.dma_rx_control_count;
}

uint32_t mock_dma_status(void) {
  return state.dma.status;
}

size_t mock_dma_status_read_count(void) {
  return state.dma_status_read_count;
}

size_t mock_dma_status_clear_write_count(void) {
  return state.dma_status_clear_count;
}

uint32_t mock_dma_last_status_clear(void) {
  return state.last_dma_status_clear;
}

void mock_spi_dma_set_interrupts_enabled(bool enabled) {
  state.interrupts_enabled = enabled;
}

bool mock_spi_dma_interrupts_enabled(void) {
  return state.interrupts_enabled;
}

size_t mock_spi_dma_irq_save_count(void) {
  return state.irq_save_count;
}

size_t mock_spi_dma_irq_restore_count(void) {
  return state.irq_restore_count;
}

uint32_t mock_spi_dma_last_irq_restore(void) {
  return state.last_irq_restore;
}

uintptr_t spi_dma_buffer_address(const void *buffer) {
  return (uintptr_t)buffer;
}

uintptr_t spi0_data_dma_address(const volatile spi0_registers_t *spi) {
  return is_spi(spi) ? (uintptr_t)&state.spi.data : 0u;
}

void spi0_write_control(
  volatile spi0_registers_t *spi,
  uint32_t value
) {
  if (!is_spi(spi)) return;

  record_event(MOCK_SPI_DMA_EVENT_SPI_CONTROL);
  state.spi.control = value;
  if (state.spi_control_count < MOCK_SPI_DMA_HISTORY_CAPACITY) {
    state.spi_control_history[state.spi_control_count] = value;
  }
  state.spi_control_count++;
}

void spi0_write_clock_divider(
  volatile spi0_registers_t *spi,
  uint32_t value
) {
  if (!is_spi(spi)) return;

  record_event(MOCK_SPI_DMA_EVENT_SPI_CLOCK_DIVIDER);
  state.spi.clock_divider = value;
  state.spi_clock_divider_count++;
}

void spi0_write_chip_select(
  volatile spi0_registers_t *spi,
  uint32_t value
) {
  if (!is_spi(spi)) return;

  record_event(MOCK_SPI_DMA_EVENT_SPI_CHIP_SELECT);
  state.spi.chip_select = value;
  if (state.spi_chip_select_count < MOCK_SPI_DMA_HISTORY_CAPACITY) {
    state.spi_chip_select_history[state.spi_chip_select_count] = value;
  }
  state.spi_chip_select_count++;
}

void dma0_write_tx_source(
  volatile dma0_registers_t *dma,
  uintptr_t value
) {
  if (!is_dma(dma)) return;

  record_event(MOCK_SPI_DMA_EVENT_DMA_TX_SOURCE);
  state.dma.tx_source = value;
}

void dma0_write_tx_destination(
  volatile dma0_registers_t *dma,
  uintptr_t value
) {
  if (!is_dma(dma)) return;

  record_event(MOCK_SPI_DMA_EVENT_DMA_TX_DESTINATION);
  state.dma.tx_destination = value;
}

void dma0_write_tx_count(
  volatile dma0_registers_t *dma,
  uint32_t value
) {
  if (!is_dma(dma)) return;

  record_event(MOCK_SPI_DMA_EVENT_DMA_TX_COUNT);
  state.dma.tx_count = value;
}

void dma0_write_tx_control(
  volatile dma0_registers_t *dma,
  uint32_t value
) {
  if (!is_dma(dma)) return;

  record_event(MOCK_SPI_DMA_EVENT_DMA_TX_CONTROL);
  state.dma.tx_control = value;
  state.dma_tx_control_count++;
}

void dma0_write_rx_source(
  volatile dma0_registers_t *dma,
  uintptr_t value
) {
  if (!is_dma(dma)) return;

  record_event(MOCK_SPI_DMA_EVENT_DMA_RX_SOURCE);
  state.dma.rx_source = value;
}

void dma0_write_rx_destination(
  volatile dma0_registers_t *dma,
  uintptr_t value
) {
  if (!is_dma(dma)) return;

  record_event(MOCK_SPI_DMA_EVENT_DMA_RX_DESTINATION);
  state.dma.rx_destination = value;
}

void dma0_write_rx_count(
  volatile dma0_registers_t *dma,
  uint32_t value
) {
  if (!is_dma(dma)) return;

  record_event(MOCK_SPI_DMA_EVENT_DMA_RX_COUNT);
  state.dma.rx_count = value;
}

void dma0_write_rx_control(
  volatile dma0_registers_t *dma,
  uint32_t value
) {
  if (!is_dma(dma)) return;

  record_event(MOCK_SPI_DMA_EVENT_DMA_RX_CONTROL);
  state.dma.rx_control = value;
  state.dma_rx_control_count++;
}

uint32_t dma0_read_status(const volatile dma0_registers_t *dma) {
  if (!is_dma(dma)) return 0u;

  state.dma_status_read_count++;
  return state.dma.status;
}

void dma0_write_status_clear(
  volatile dma0_registers_t *dma,
  uint32_t value
) {
  if (!is_dma(dma)) return;

  record_event(MOCK_SPI_DMA_EVENT_DMA_STATUS_CLEAR);
  state.dma.status_clear = value;
  state.dma_status_clear_count++;
  state.last_dma_status_clear = value;
  state.dma.status &= ~(value & DMA0_STATUS_ALL);
}

uint32_t spi_dma_irq_save_disable(void) {
  const uint32_t previous = state.interrupts_enabled ? UINT32_C(1) : 0u;
  state.interrupts_enabled = false;
  state.irq_save_count++;
  return previous;
}

void spi_dma_irq_restore(uint32_t restore_state) {
  state.last_irq_restore = restore_state;
  state.interrupts_enabled = restore_state != 0u;
  state.irq_restore_count++;
}

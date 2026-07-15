#include "mock_uart.h"

#define MOCK_UART_FIFO_CAPACITY 32u
#define MOCK_UART_HISTORY_CAPACITY 64u

typedef struct {
  uart0_registers_t registers;
  uint8_t rx_fifo[MOCK_UART_FIFO_CAPACITY];
  uint8_t tx_history[MOCK_UART_HISTORY_CAPACITY];
  uint32_t control_history[MOCK_UART_HISTORY_CAPACITY];
  size_t rx_head;
  size_t rx_tail;
  size_t rx_count;
  size_t tx_count;
  size_t control_count;
  size_t baud_write_count;
  size_t status_read_count;
  size_t data_read_count;
  size_t error_clear_write_count;
  size_t irq_save_count;
  size_t irq_restore_count;
  uint32_t error_status;
  uint32_t last_error_clear;
  uint32_t last_irq_restore;
  bool interrupts_enabled;
} mock_uart_state_t;

static mock_uart_state_t state;

static bool is_uart0(const volatile uart0_registers_t *uart) {
  return uart == &state.registers;
}

static size_t advance_index(size_t index, size_t capacity) {
  index++;
  return index == capacity ? 0u : index;
}

void mock_uart_reset(void) {
  state = (mock_uart_state_t){ 0 };
  state.interrupts_enabled = true;
}

volatile uart0_registers_t *mock_uart0(void) {
  return &state.registers;
}

bool mock_uart_push_rx(uint8_t value) {
  if (state.rx_count == MOCK_UART_FIFO_CAPACITY) return false;
  state.rx_fifo[state.rx_head] = value;
  state.rx_head = advance_index(state.rx_head, MOCK_UART_FIFO_CAPACITY);
  state.rx_count++;
  return true;
}

size_t mock_uart_rx_pending(void) {
  return state.rx_count;
}

void mock_uart_raise_errors(uint32_t error_bits) {
  state.error_status |= error_bits & UART0_STATUS_ERROR_MASK;
}

uint32_t mock_uart_control(void) {
  return state.registers.control;
}

uint32_t mock_uart_control_at(size_t index) {
  return index < state.control_count ? state.control_history[index] : 0u;
}

size_t mock_uart_control_write_count(void) {
  return state.control_count;
}

uint32_t mock_uart_baud(void) {
  return state.registers.baud;
}

size_t mock_uart_baud_write_count(void) {
  return state.baud_write_count;
}

size_t mock_uart_status_read_count(void) {
  return state.status_read_count;
}

size_t mock_uart_data_read_count(void) {
  return state.data_read_count;
}

size_t mock_uart_data_write_count(void) {
  return state.tx_count;
}

uint8_t mock_uart_data_write_at(size_t index) {
  return index < state.tx_count ? state.tx_history[index] : 0u;
}

size_t mock_uart_error_clear_write_count(void) {
  return state.error_clear_write_count;
}

uint32_t mock_uart_last_error_clear(void) {
  return state.last_error_clear;
}

void mock_uart_set_interrupts_enabled(bool enabled) {
  state.interrupts_enabled = enabled;
}

bool mock_uart_interrupts_enabled(void) {
  return state.interrupts_enabled;
}

size_t mock_uart_irq_save_count(void) {
  return state.irq_save_count;
}

size_t mock_uart_irq_restore_count(void) {
  return state.irq_restore_count;
}

uint32_t mock_uart_last_irq_restore(void) {
  return state.last_irq_restore;
}

uint32_t uart0_host_read(
  const volatile uart0_registers_t *uart,
  uint32_t offset
) {
  if (!is_uart0(uart)) return 0u;

  if (offset == UART0_CONTROL_OFFSET) return state.registers.control;
  if (offset == UART0_STATUS_OFFSET) {
    uint32_t status = UART0_STATUS_TX_EMPTY | state.error_status;
    state.status_read_count++;
    if (state.rx_count != 0u) status |= UART0_STATUS_RX_READY;
    return status;
  }
  if (offset == UART0_DATA_OFFSET) {
    uint8_t value = 0u;
    state.data_read_count++;
    if (state.rx_count != 0u) {
      value = state.rx_fifo[state.rx_tail];
      state.rx_tail = advance_index(state.rx_tail, MOCK_UART_FIFO_CAPACITY);
      state.rx_count--;
    }
    return value;
  }
  return 0u;
}

void uart0_host_write(
  volatile uart0_registers_t *uart,
  uint32_t offset,
  uint32_t value
) {
  if (!is_uart0(uart)) return;

  if (offset == UART0_CONTROL_OFFSET) {
    state.registers.control = value;
    if (state.control_count < MOCK_UART_HISTORY_CAPACITY) {
      state.control_history[state.control_count] = value;
    }
    state.control_count++;
    return;
  }
  if (offset == UART0_BAUD_OFFSET) {
    state.registers.baud = value;
    state.baud_write_count++;
    return;
  }
  if (offset == UART0_DATA_OFFSET) {
    state.registers.data = value;
    if (state.tx_count < MOCK_UART_HISTORY_CAPACITY) {
      state.tx_history[state.tx_count] = (uint8_t)value;
    }
    state.tx_count++;
    return;
  }
  if (offset == UART0_ERROR_CLEAR_OFFSET) {
    state.registers.error_clear = value;
    state.error_clear_write_count++;
    state.last_error_clear = value;
    state.error_status &= ~(value & UART0_STATUS_ERROR_MASK);
  }
}

uint32_t uart0_irq_save_disable(void) {
  const uint32_t previous = state.interrupts_enabled ? UINT32_C(1) : 0u;
  state.interrupts_enabled = false;
  state.irq_save_count++;
  return previous;
}

void uart0_irq_restore(uint32_t restore_state) {
  state.last_irq_restore = restore_state;
  state.interrupts_enabled = restore_state != 0u;
  state.irq_restore_count++;
}

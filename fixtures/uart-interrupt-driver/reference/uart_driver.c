#include "uart_driver.h"

static bool driver_is_ready(const uart_driver_t *driver) {
  return driver != NULL && driver->initialized && driver->uart != NULL;
}

static uint8_t advance_index(uint8_t index) {
  index++;
  return index == UART_DRIVER_BUFFER_CAPACITY ? 0u : index;
}

static void disable_tx_interrupt(uart_driver_t *driver) {
  const uint32_t control = uart0_read_control(driver->uart);
  if ((control & UART0_CONTROL_TX_IRQ_ENABLE) != 0u) {
    uart0_write_control(
      driver->uart,
      control & ~UART0_CONTROL_TX_IRQ_ENABLE
    );
  }
}

static void service_receive(uart_driver_t *driver) {
  const uint8_t value = uart0_read_data(driver->uart);
  if (driver->rx_count == UART_DRIVER_BUFFER_CAPACITY) {
    driver->rx_dropped++;
    return;
  }
  driver->rx_buffer[driver->rx_head] = value;
  driver->rx_head = advance_index(driver->rx_head);
  driver->rx_count++;
}

static void service_transmit(uart_driver_t *driver) {
  if (driver->tx_count != 0u) {
    const uint8_t value = driver->tx_buffer[driver->tx_tail];
    driver->tx_tail = advance_index(driver->tx_tail);
    driver->tx_count--;
    uart0_write_data(driver->uart, value);
  }
  if (driver->tx_count == 0u) {
    disable_tx_interrupt(driver);
  }
}

bool uart_driver_init(
  uart_driver_t *driver,
  volatile uart0_registers_t *uart
) {
  if (driver == NULL || uart == NULL) return false;

  *driver = (uart_driver_t){ 0 };
  driver->uart = uart;
  uart0_write_control(uart, 0u);
  uart0_write_baud(uart, UART0_BAUD_DIVISOR);
  uart0_write_error_clear(uart, UART0_STATUS_ERROR_MASK);
  uart0_write_control(
    uart,
    UART0_CONTROL_ENABLE |
      UART0_CONTROL_RX_ENABLE |
      UART0_CONTROL_TX_ENABLE |
      UART0_CONTROL_RX_IRQ_ENABLE
  );
  driver->initialized = true;
  return true;
}

size_t uart_driver_write(
  uart_driver_t *driver,
  const uint8_t *data,
  size_t length
) {
  if (!driver_is_ready(driver) || data == NULL || length == 0u) return 0u;

  const uint32_t write_interrupt_state = uart0_irq_save_disable();
  size_t written = 0u;
  while (
    written < length && driver->tx_count < UART_DRIVER_BUFFER_CAPACITY
  ) {
    driver->tx_buffer[driver->tx_head] = data[written];
    driver->tx_head = advance_index(driver->tx_head);
    driver->tx_count++;
    written++;
  }
  if (written != 0u) {
    const uint32_t control = uart0_read_control(driver->uart);
    uart0_write_control(
      driver->uart,
      control | UART0_CONTROL_TX_IRQ_ENABLE
    );
  }
  uart0_irq_restore(write_interrupt_state);
  return written;
}

bool uart_driver_read(uart_driver_t *driver, uint8_t *value) {
  if (!driver_is_ready(driver) || value == NULL) return false;

  const uint32_t read_interrupt_state = uart0_irq_save_disable();
  bool read = false;
  if (driver->rx_count != 0u) {
    *value = driver->rx_buffer[driver->rx_tail];
    driver->rx_tail = advance_index(driver->rx_tail);
    driver->rx_count--;
    read = true;
  }
  uart0_irq_restore(read_interrupt_state);
  return read;
}

void uart_driver_irq(uart_driver_t *driver) {
  if (!driver_is_ready(driver)) return;

  const uint32_t status = uart0_read_status(driver->uart);
  const uint32_t errors = status & UART0_STATUS_ERROR_MASK;
  if (errors != 0u) {
    driver->error_flags |= errors;
    uart0_write_error_clear(driver->uart, errors);
  }
  if ((status & UART0_STATUS_RX_READY) != 0u) {
    service_receive(driver);
  }
  if ((status & UART0_STATUS_TX_EMPTY) != 0u) {
    service_transmit(driver);
  }
}

uint32_t uart_driver_take_errors(uart_driver_t *driver) {
  if (!driver_is_ready(driver)) return 0u;

  const uint32_t error_interrupt_state = uart0_irq_save_disable();
  const uint32_t errors = driver->error_flags;
  driver->error_flags = 0u;
  uart0_irq_restore(error_interrupt_state);
  return errors;
}

uint32_t uart_driver_rx_dropped(uart_driver_t *driver) {
  if (!driver_is_ready(driver)) return 0u;

  const uint32_t dropped_interrupt_state = uart0_irq_save_disable();
  const uint32_t dropped = driver->rx_dropped;
  uart0_irq_restore(dropped_interrupt_state);
  return dropped;
}

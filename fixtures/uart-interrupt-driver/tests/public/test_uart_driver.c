#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "mock_uart.h"
#include "uart_driver.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
              __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

static uint32_t initialized_control(void) {
  return UART0_CONTROL_ENABLE |
    UART0_CONTROL_RX_ENABLE |
    UART0_CONTROL_TX_ENABLE |
    UART0_CONTROL_RX_IRQ_ENABLE;
}

static bool initialize(uart_driver_t *driver) {
  return uart_driver_init(driver, mock_uart0());
}

static bool test_initialization_sequence_and_reset(void) {
  uart_driver_t driver = {
    .rx_count = UINT8_C(3),
    .tx_count = UINT8_C(2),
    .rx_dropped = UINT32_C(9),
    .error_flags = UART0_STATUS_ERROR_MASK,
    .initialized = true,
  };
  const uint8_t tx_byte = UINT8_C(0x45);
  size_t transmitted;

  mock_uart_reset();
  CHECK(!uart_driver_init(NULL, mock_uart0()));
  CHECK(!uart_driver_init(&driver, NULL));
  CHECK(mock_uart_control_write_count() == 0u);
  CHECK(mock_uart_baud_write_count() == 0u);
  CHECK(mock_uart_error_clear_write_count() == 0u);
  CHECK(mock_uart_irq_save_count() == 0u);

  CHECK(initialize(&driver));
  CHECK(driver.initialized);
  CHECK(driver.uart == mock_uart0());
  CHECK(driver.rx_count == 0u);
  CHECK(driver.tx_count == 0u);
  CHECK(driver.rx_dropped == 0u);
  CHECK(driver.error_flags == 0u);
  CHECK(mock_uart_control_write_count() == 2u);
  CHECK(mock_uart_control_at(0u) == 0u);
  CHECK(mock_uart_control_at(1u) == initialized_control());
  CHECK(mock_uart_control() == initialized_control());
  CHECK(mock_uart_baud_write_count() == 1u);
  CHECK(mock_uart_baud() == UART0_BAUD_DIVISOR);
  CHECK(mock_uart_error_clear_write_count() == 1u);
  CHECK(mock_uart_last_error_clear() == UART0_STATUS_ERROR_MASK);
  CHECK(mock_uart_irq_save_count() == 0u);

  CHECK(uart_driver_write(&driver, &tx_byte, 1u) == 1u);
  uart_driver_irq(&driver);
  CHECK(mock_uart_data_write_count() == 1u);
  CHECK(mock_uart_push_rx(UINT8_C(0xA1)));
  uart_driver_irq(&driver);
  mock_uart_raise_errors(UART0_STATUS_OVERRUN);
  uart_driver_irq(&driver);
  CHECK(driver.rx_count == 1u);
  CHECK(driver.error_flags == UART0_STATUS_OVERRUN);

  CHECK(initialize(&driver));
  CHECK(driver.rx_count == 0u);
  CHECK(driver.tx_count == 0u);
  CHECK(driver.rx_dropped == 0u);
  CHECK(driver.error_flags == 0u);
  CHECK(mock_uart_control() == initialized_control());
  transmitted = mock_uart_data_write_count();
  uart_driver_irq(&driver);
  CHECK(mock_uart_data_write_count() == transmitted);
  return true;
}

static bool test_transmit_capacity_order_and_irq_bound(void) {
  uint8_t values[UART_DRIVER_BUFFER_CAPACITY + UINT8_C(2)];
  uart_driver_t driver = { 0 };
  size_t control_writes;

  for (size_t index = 0u; index < sizeof(values); index++) {
    values[index] = (uint8_t)(UINT8_C(0x30) + index);
  }
  mock_uart_reset();
  CHECK(initialize(&driver));

  CHECK(
    uart_driver_write(&driver, values, sizeof(values)) ==
      UART_DRIVER_BUFFER_CAPACITY
  );
  CHECK(driver.tx_count == UART_DRIVER_BUFFER_CAPACITY);
  CHECK(mock_uart_data_write_count() == 0u);
  CHECK(
    (mock_uart_control() & UART0_CONTROL_TX_IRQ_ENABLE) != 0u
  );
  CHECK(mock_uart_irq_save_count() == 1u);
  CHECK(mock_uart_irq_restore_count() == 1u);
  CHECK(uart_driver_write(&driver, values, 1u) == 0u);

  for (size_t index = 0u; index < UART_DRIVER_BUFFER_CAPACITY; index++) {
    uart_driver_irq(&driver);
    CHECK(mock_uart_data_write_count() == index + 1u);
    CHECK(mock_uart_data_write_at(index) == values[index]);
    CHECK(mock_uart_status_read_count() == index + 1u);
    if (index + 1u < UART_DRIVER_BUFFER_CAPACITY) {
      CHECK(driver.tx_count == UART_DRIVER_BUFFER_CAPACITY - index - 1u);
      CHECK(
        (mock_uart_control() & UART0_CONTROL_TX_IRQ_ENABLE) != 0u
      );
    }
  }
  CHECK(driver.tx_count == 0u);
  CHECK(
    (mock_uart_control() & UART0_CONTROL_TX_IRQ_ENABLE) == 0u
  );
  control_writes = mock_uart_control_write_count();
  uart_driver_irq(&driver);
  CHECK(mock_uart_data_write_count() == UART_DRIVER_BUFFER_CAPACITY);
  CHECK(mock_uart_control_write_count() == control_writes);
  return true;
}

static bool test_receive_order_one_byte_per_irq_and_overflow(void) {
  uart_driver_t driver = { 0 };
  uint8_t value = 0u;

  mock_uart_reset();
  CHECK(initialize(&driver));
  CHECK(mock_uart_push_rx(UINT8_C(0x11)));
  CHECK(mock_uart_push_rx(UINT8_C(0x22)));
  uart_driver_irq(&driver);
  CHECK(mock_uart_rx_pending() == 1u);
  CHECK(driver.rx_count == 1u);
  uart_driver_irq(&driver);
  CHECK(mock_uart_rx_pending() == 0u);
  CHECK(driver.rx_count == 2u);

  for (size_t index = 0u;
       index < UART_DRIVER_BUFFER_CAPACITY - UINT8_C(2);
       index++) {
    CHECK(mock_uart_push_rx((uint8_t)(UINT8_C(0x30) + index)));
    uart_driver_irq(&driver);
  }
  CHECK(driver.rx_count == UART_DRIVER_BUFFER_CAPACITY);
  CHECK(mock_uart_push_rx(UINT8_C(0xFE)));
  uart_driver_irq(&driver);
  CHECK(driver.rx_count == UART_DRIVER_BUFFER_CAPACITY);
  CHECK(mock_uart_data_read_count() == UART_DRIVER_BUFFER_CAPACITY + 1u);
  CHECK(uart_driver_rx_dropped(&driver) == 1u);

  for (size_t index = 0u; index < UART_DRIVER_BUFFER_CAPACITY; index++) {
    uint8_t expected = (uint8_t)(UINT8_C(0x30) + index - 2u);
    if (index == 0u) expected = UINT8_C(0x11);
    if (index == 1u) expected = UINT8_C(0x22);
    CHECK(uart_driver_read(&driver, &value));
    CHECK(value == expected);
  }
  CHECK(!uart_driver_read(&driver, &value));
  return true;
}

static bool test_error_acknowledgement_and_take(void) {
  uart_driver_t driver = { 0 };
  uint8_t value = 0u;
  size_t irq_saves;

  mock_uart_reset();
  CHECK(initialize(&driver));
  CHECK(mock_uart_push_rx(UINT8_C(0xA5)));
  mock_uart_raise_errors(UART0_STATUS_OVERRUN | UART0_STATUS_FRAMING);
  irq_saves = mock_uart_irq_save_count();
  uart_driver_irq(&driver);
  CHECK(mock_uart_irq_save_count() == irq_saves);
  CHECK(mock_uart_irq_restore_count() == irq_saves);
  CHECK(mock_uart_error_clear_write_count() == 2u);
  CHECK(mock_uart_last_error_clear() == UART0_STATUS_ERROR_MASK);
  CHECK(uart_driver_read(&driver, &value));
  CHECK(value == UINT8_C(0xA5));
  CHECK(uart_driver_take_errors(&driver) == UART0_STATUS_ERROR_MASK);
  CHECK(uart_driver_take_errors(&driver) == 0u);
  return true;
}

static bool test_foreground_critical_sections_and_invalid_inputs(void) {
  uart_driver_t driver = { 0 };
  uart_driver_t uninitialized = { 0 };
  const uint8_t byte = UINT8_C(0x9C);
  uint8_t value = 0u;
  size_t saves;
  size_t restores;

  mock_uart_reset();
  CHECK(uart_driver_write(NULL, &byte, 1u) == 0u);
  CHECK(!uart_driver_read(NULL, &value));
  CHECK(uart_driver_take_errors(NULL) == 0u);
  CHECK(uart_driver_rx_dropped(NULL) == 0u);
  uart_driver_irq(NULL);
  CHECK(uart_driver_write(&uninitialized, &byte, 1u) == 0u);
  CHECK(!uart_driver_read(&uninitialized, &value));
  uart_driver_irq(&uninitialized);
  CHECK(mock_uart_irq_save_count() == 0u);
  CHECK(mock_uart_irq_restore_count() == 0u);
  CHECK(mock_uart_status_read_count() == 0u);

  CHECK(initialize(&driver));
  saves = mock_uart_irq_save_count();
  restores = mock_uart_irq_restore_count();
  CHECK(uart_driver_write(&driver, NULL, 1u) == 0u);
  CHECK(uart_driver_write(&driver, &byte, 0u) == 0u);
  CHECK(!uart_driver_read(&driver, NULL));
  CHECK(mock_uart_irq_save_count() == saves);
  CHECK(mock_uart_irq_restore_count() == restores);

  mock_uart_set_interrupts_enabled(false);
  CHECK(uart_driver_write(&driver, &byte, 1u) == 1u);
  CHECK(!mock_uart_interrupts_enabled());
  CHECK(mock_uart_last_irq_restore() == 0u);
  CHECK(!uart_driver_read(&driver, &value));
  CHECK(!mock_uart_interrupts_enabled());
  CHECK(uart_driver_take_errors(&driver) == 0u);
  CHECK(!mock_uart_interrupts_enabled());
  CHECK(uart_driver_rx_dropped(&driver) == 0u);
  CHECK(!mock_uart_interrupts_enabled());

  saves = mock_uart_irq_save_count();
  restores = mock_uart_irq_restore_count();
  mock_uart_set_interrupts_enabled(true);
  uart_driver_irq(&driver);
  CHECK(mock_uart_irq_save_count() == saves);
  CHECK(mock_uart_irq_restore_count() == restores);
  CHECK(mock_uart_interrupts_enabled());
  CHECK(mock_uart_data_write_count() == 1u);
  CHECK(uart_driver_write(&driver, &byte, 1u) == 1u);
  CHECK(mock_uart_interrupts_enabled());
  CHECK(mock_uart_last_irq_restore() == UINT32_C(1));
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "initialization sequence and reset", test_initialization_sequence_and_reset },
    { "transmit capacity order and bound", test_transmit_capacity_order_and_irq_bound },
    { "receive order, bound, and overflow", test_receive_order_one_byte_per_irq_and_overflow },
    { "error acknowledgement", test_error_acknowledgement_and_take },
    { "foreground critical sections", test_foreground_critical_sections_and_invalid_inputs },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) return 1;
    printf("ok - %s\n", tests[index].name);
  }
  return 0;
}

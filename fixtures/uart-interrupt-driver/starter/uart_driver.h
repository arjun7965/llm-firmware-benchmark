#ifndef UART_DRIVER_H
#define UART_DRIVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_uart.h"

#define UART_DRIVER_BUFFER_CAPACITY UINT8_C(8)
#define UART0_BAUD_DIVISOR UINT32_C(26)

typedef struct {
  volatile uart0_registers_t *uart;
  uint8_t rx_buffer[UART_DRIVER_BUFFER_CAPACITY];
  uint8_t tx_buffer[UART_DRIVER_BUFFER_CAPACITY];
  uint8_t rx_head;
  uint8_t rx_tail;
  uint8_t rx_count;
  uint8_t tx_head;
  uint8_t tx_tail;
  uint8_t tx_count;
  uint32_t rx_dropped;
  uint32_t error_flags;
  bool initialized;
} uart_driver_t;

_Static_assert(
  UART_DRIVER_BUFFER_CAPACITY > 0u,
  "UART buffers must have capacity"
);

bool uart_driver_init(
  uart_driver_t *driver,
  volatile uart0_registers_t *uart
);
size_t uart_driver_write(
  uart_driver_t *driver,
  const uint8_t *data,
  size_t length
);
bool uart_driver_read(uart_driver_t *driver, uint8_t *value);
void uart_driver_irq(uart_driver_t *driver);
uint32_t uart_driver_take_errors(uart_driver_t *driver);
uint32_t uart_driver_rx_dropped(uart_driver_t *driver);

#endif

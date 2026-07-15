#ifndef FIXTURE_UART_H
#define FIXTURE_UART_H

#include <stddef.h>
#include <stdint.h>

#define UART0_BASE_ADDRESS UINT32_C(0x40011000)

#define UART0_CONTROL_ENABLE UINT32_C(1)
#define UART0_CONTROL_RX_ENABLE (UINT32_C(1) << 1)
#define UART0_CONTROL_TX_ENABLE (UINT32_C(1) << 2)
#define UART0_CONTROL_RX_IRQ_ENABLE (UINT32_C(1) << 3)
#define UART0_CONTROL_TX_IRQ_ENABLE (UINT32_C(1) << 4)

#define UART0_STATUS_RX_READY UINT32_C(1)
#define UART0_STATUS_TX_EMPTY (UINT32_C(1) << 1)
#define UART0_STATUS_OVERRUN (UINT32_C(1) << 2)
#define UART0_STATUS_FRAMING (UINT32_C(1) << 3)
#define UART0_STATUS_ERROR_MASK \
  (UART0_STATUS_OVERRUN | UART0_STATUS_FRAMING)

#define UART0_CONTROL_OFFSET UINT32_C(0x00)
#define UART0_BAUD_OFFSET UINT32_C(0x04)
#define UART0_STATUS_OFFSET UINT32_C(0x08)
#define UART0_DATA_OFFSET UINT32_C(0x0C)
#define UART0_ERROR_CLEAR_OFFSET UINT32_C(0x10)

typedef struct {
  volatile uint32_t control;
  volatile uint32_t baud;
  volatile uint32_t status;
  volatile uint32_t data;
  volatile uint32_t error_clear;
} uart0_registers_t;

_Static_assert(
  offsetof(uart0_registers_t, control) == UART0_CONTROL_OFFSET,
  "unexpected UART0 control offset"
);
_Static_assert(
  offsetof(uart0_registers_t, baud) == UART0_BAUD_OFFSET,
  "unexpected UART0 baud offset"
);
_Static_assert(
  offsetof(uart0_registers_t, status) == UART0_STATUS_OFFSET,
  "unexpected UART0 status offset"
);
_Static_assert(
  offsetof(uart0_registers_t, data) == UART0_DATA_OFFSET,
  "unexpected UART0 data offset"
);
_Static_assert(
  offsetof(uart0_registers_t, error_clear) == UART0_ERROR_CLEAR_OFFSET,
  "unexpected UART0 error-clear offset"
);
_Static_assert(sizeof(uart0_registers_t) == 20u, "unexpected UART0 size");

#if defined(UART0_HOST_TEST)
uint32_t uart0_host_read(
  const volatile uart0_registers_t *uart,
  uint32_t offset
);
void uart0_host_write(
  volatile uart0_registers_t *uart,
  uint32_t offset,
  uint32_t value
);
#endif

static inline uint32_t uart0_read_control(
  const volatile uart0_registers_t *uart
) {
#if defined(UART0_HOST_TEST)
  return uart0_host_read(uart, UART0_CONTROL_OFFSET);
#else
  return uart->control;
#endif
}

static inline uint32_t uart0_read_status(
  const volatile uart0_registers_t *uart
) {
#if defined(UART0_HOST_TEST)
  return uart0_host_read(uart, UART0_STATUS_OFFSET);
#else
  return uart->status;
#endif
}

static inline uint8_t uart0_read_data(volatile uart0_registers_t *uart) {
#if defined(UART0_HOST_TEST)
  return (uint8_t)uart0_host_read(uart, UART0_DATA_OFFSET);
#else
  return (uint8_t)uart->data;
#endif
}

static inline void uart0_write_control(
  volatile uart0_registers_t *uart,
  uint32_t value
) {
#if defined(UART0_HOST_TEST)
  uart0_host_write(uart, UART0_CONTROL_OFFSET, value);
#else
  uart->control = value;
#endif
}

static inline void uart0_write_baud(
  volatile uart0_registers_t *uart,
  uint32_t value
) {
#if defined(UART0_HOST_TEST)
  uart0_host_write(uart, UART0_BAUD_OFFSET, value);
#else
  uart->baud = value;
#endif
}

static inline void uart0_write_data(
  volatile uart0_registers_t *uart,
  uint8_t value
) {
#if defined(UART0_HOST_TEST)
  uart0_host_write(uart, UART0_DATA_OFFSET, value);
#else
  uart->data = value;
#endif
}

static inline void uart0_write_error_clear(
  volatile uart0_registers_t *uart,
  uint32_t value
) {
#if defined(UART0_HOST_TEST)
  uart0_host_write(uart, UART0_ERROR_CLEAR_OFFSET, value);
#else
  uart->error_clear = value;
#endif
}

uint32_t uart0_irq_save_disable(void);
void uart0_irq_restore(uint32_t state);

#endif

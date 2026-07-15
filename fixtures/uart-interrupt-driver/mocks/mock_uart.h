#ifndef MOCK_UART_H
#define MOCK_UART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_uart.h"

void mock_uart_reset(void);
volatile uart0_registers_t *mock_uart0(void);

bool mock_uart_push_rx(uint8_t value);
size_t mock_uart_rx_pending(void);
void mock_uart_raise_errors(uint32_t error_bits);

uint32_t mock_uart_control(void);
uint32_t mock_uart_control_at(size_t index);
size_t mock_uart_control_write_count(void);
uint32_t mock_uart_baud(void);
size_t mock_uart_baud_write_count(void);
size_t mock_uart_status_read_count(void);
size_t mock_uart_data_read_count(void);
size_t mock_uart_data_write_count(void);
uint8_t mock_uart_data_write_at(size_t index);
size_t mock_uart_error_clear_write_count(void);
uint32_t mock_uart_last_error_clear(void);

void mock_uart_set_interrupts_enabled(bool enabled);
bool mock_uart_interrupts_enabled(void);
size_t mock_uart_irq_save_count(void);
size_t mock_uart_irq_restore_count(void);
uint32_t mock_uart_last_irq_restore(void);

#endif

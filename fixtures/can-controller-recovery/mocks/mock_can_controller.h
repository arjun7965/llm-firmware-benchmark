#ifndef MOCK_CAN_CONTROLLER_H
#define MOCK_CAN_CONTROLLER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "can_controller.h"

typedef enum {
  MOCK_CAN_EVENT_STATUS_READ,
  MOCK_CAN_EVENT_STATUS_CLEAR,
  MOCK_CAN_EVENT_CONTROL_WRITE,
  MOCK_CAN_EVENT_BIT_TIMING_WRITE,
  MOCK_CAN_EVENT_ACCEPTANCE_FILTER_WRITE,
  MOCK_CAN_EVENT_TX_IDENTIFIER_WRITE,
  MOCK_CAN_EVENT_TX_DLC_WRITE,
  MOCK_CAN_EVENT_TX_DATA_WRITE,
  MOCK_CAN_EVENT_RX_IDENTIFIER_READ,
  MOCK_CAN_EVENT_RX_DLC_READ,
  MOCK_CAN_EVENT_RX_DATA_READ,
  MOCK_CAN_EVENT_COUNT,
} mock_can_event_t;

void mock_can_reset(void);
volatile can0_registers_t *mock_can0(void);

bool mock_can_signal_tx_complete(void);
bool mock_can_signal_tx_error(void);
bool mock_can_inject_rx(
  uint16_t identifier,
  uint8_t dlc,
  const uint8_t *data
);
void mock_can_set_status(uint32_t status);
void mock_can_set_recovery_ready(bool ready);

size_t mock_can_event_count(void);
mock_can_event_t mock_can_event_at(size_t index);
uint32_t mock_can_event_value(size_t index);

uint32_t mock_can_status(void);
size_t mock_can_status_read_count(void);
size_t mock_can_status_clear_write_count(void);
uint32_t mock_can_last_status_clear(void);
uint32_t mock_can_control(void);
size_t mock_can_control_write_count(void);
uint32_t mock_can_control_at(size_t index);
uint32_t mock_can_bit_timing(void);
size_t mock_can_bit_timing_write_count(void);
uint32_t mock_can_acceptance_filter(void);
size_t mock_can_acceptance_filter_write_count(void);
uint16_t mock_can_tx_identifier(void);
uint8_t mock_can_tx_dlc(void);
size_t mock_can_tx_data_write_count(void);
uint8_t mock_can_tx_data_at(size_t index);

void mock_can_set_interrupts_enabled(bool enabled);
bool mock_can_interrupts_enabled(void);
size_t mock_can_irq_save_count(void);
size_t mock_can_irq_restore_count(void);
uint32_t mock_can_last_irq_restore(void);
bool mock_can_invalid_access(void);

#endif

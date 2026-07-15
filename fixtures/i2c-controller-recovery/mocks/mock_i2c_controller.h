#ifndef MOCK_I2C_CONTROLLER_H
#define MOCK_I2C_CONTROLLER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_i2c_controller.h"

typedef enum {
  MOCK_I2C_EVENT_STATUS_READ,
  MOCK_I2C_EVENT_STATUS_CLEAR,
  MOCK_I2C_EVENT_DATA_WRITE,
  MOCK_I2C_EVENT_CONTROL_WRITE,
  MOCK_I2C_EVENT_COUNT,
} mock_i2c_event_t;

void mock_i2c_reset(void);
volatile i2c0_registers_t *mock_i2c0(void);

void mock_i2c_set_status(uint32_t status);
bool mock_i2c_signal_start(void);
bool mock_i2c_signal_address_ack(void);
bool mock_i2c_signal_data_ack(void);
bool mock_i2c_signal_nack(void);
bool mock_i2c_signal_arbitration_lost(void);
bool mock_i2c_signal_bus_error(void);

uint32_t mock_i2c_status(void);
uint32_t mock_i2c_control(void);
size_t mock_i2c_control_write_count(void);
uint32_t mock_i2c_control_at(size_t index);
size_t mock_i2c_status_read_count(void);
size_t mock_i2c_status_clear_write_count(void);
uint32_t mock_i2c_status_clear_at(size_t index);
size_t mock_i2c_data_write_count(void);
uint8_t mock_i2c_data_at(size_t index);
size_t mock_i2c_event_count(void);
mock_i2c_event_t mock_i2c_event_at(size_t index);
uint32_t mock_i2c_event_value(size_t index);
bool mock_i2c_invalid_access(void);

#endif

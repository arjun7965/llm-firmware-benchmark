#include "mock_i2c_controller.h"

#define MOCK_I2C_HISTORY_CAPACITY 128u

struct i2c0_registers {
  uint32_t reserved;
};

typedef struct {
  mock_i2c_event_t event;
  uint32_t value;
} mock_i2c_event_record_t;

typedef struct {
  struct i2c0_registers i2c;
  uint32_t status;
  uint32_t control;
  uint32_t controls[MOCK_I2C_HISTORY_CAPACITY];
  size_t control_write_count;
  uint32_t status_clears[MOCK_I2C_HISTORY_CAPACITY];
  size_t status_clear_write_count;
  uint8_t data[MOCK_I2C_HISTORY_CAPACITY];
  size_t data_write_count;
  size_t status_read_count;
  mock_i2c_event_record_t events[MOCK_I2C_HISTORY_CAPACITY];
  size_t event_count;
  bool invalid_access;
} mock_i2c_state_t;

static mock_i2c_state_t state;

static bool is_i2c(const volatile i2c0_registers_t *i2c) {
  return i2c == &state.i2c;
}

static void record_event(mock_i2c_event_t event, uint32_t value) {
  if (state.event_count < MOCK_I2C_HISTORY_CAPACITY) {
    state.events[state.event_count] = (mock_i2c_event_record_t) {
      .event = event,
      .value = value,
    };
  } else {
    state.invalid_access = true;
  }
  state.event_count++;
}

void mock_i2c_reset(void) {
  state = (mock_i2c_state_t){ 0 };
}

volatile i2c0_registers_t *mock_i2c0(void) {
  return &state.i2c;
}

void mock_i2c_set_status(uint32_t status) {
  state.status = status & I2C0_STATUS_ALL;
}

bool mock_i2c_signal_start(void) {
  state.status |= I2C0_STATUS_START;
  return true;
}

bool mock_i2c_signal_address_ack(void) {
  state.status |= I2C0_STATUS_ADDRESS_ACK;
  return true;
}

bool mock_i2c_signal_data_ack(void) {
  state.status |= I2C0_STATUS_DATA_ACK;
  return true;
}

bool mock_i2c_signal_nack(void) {
  state.status |= I2C0_STATUS_NACK;
  return true;
}

bool mock_i2c_signal_arbitration_lost(void) {
  state.status |= I2C0_STATUS_ARBITRATION_LOST;
  return true;
}

bool mock_i2c_signal_bus_error(void) {
  state.status |= I2C0_STATUS_BUS_ERROR;
  return true;
}

uint32_t mock_i2c_status(void) {
  return state.status;
}

uint32_t mock_i2c_control(void) {
  return state.control;
}

size_t mock_i2c_control_write_count(void) {
  return state.control_write_count;
}

uint32_t mock_i2c_control_at(size_t index) {
  return index < state.control_write_count && index < MOCK_I2C_HISTORY_CAPACITY
    ? state.controls[index]
    : UINT32_MAX;
}

size_t mock_i2c_status_read_count(void) {
  return state.status_read_count;
}

size_t mock_i2c_status_clear_write_count(void) {
  return state.status_clear_write_count;
}

uint32_t mock_i2c_status_clear_at(size_t index) {
  return index < state.status_clear_write_count &&
      index < MOCK_I2C_HISTORY_CAPACITY
    ? state.status_clears[index]
    : UINT32_MAX;
}

size_t mock_i2c_data_write_count(void) {
  return state.data_write_count;
}

uint8_t mock_i2c_data_at(size_t index) {
  return index < state.data_write_count && index < MOCK_I2C_HISTORY_CAPACITY
    ? state.data[index]
    : UINT8_MAX;
}

size_t mock_i2c_event_count(void) {
  return state.event_count;
}

mock_i2c_event_t mock_i2c_event_at(size_t index) {
  return index < state.event_count && index < MOCK_I2C_HISTORY_CAPACITY
    ? state.events[index].event
    : MOCK_I2C_EVENT_COUNT;
}

uint32_t mock_i2c_event_value(size_t index) {
  return index < state.event_count && index < MOCK_I2C_HISTORY_CAPACITY
    ? state.events[index].value
    : UINT32_MAX;
}

bool mock_i2c_invalid_access(void) {
  return state.invalid_access;
}

uint32_t i2c0_read_status(const volatile i2c0_registers_t *i2c) {
  if (!is_i2c(i2c)) {
    state.invalid_access = true;
    return 0u;
  }
  state.status_read_count++;
  record_event(MOCK_I2C_EVENT_STATUS_READ, state.status);
  return state.status;
}

void i2c0_write_control(volatile i2c0_registers_t *i2c, uint32_t value) {
  if (!is_i2c(i2c)) {
    state.invalid_access = true;
    return;
  }
  if (state.control_write_count < MOCK_I2C_HISTORY_CAPACITY) {
    state.controls[state.control_write_count] = value;
  } else {
    state.invalid_access = true;
  }
  state.control_write_count++;
  state.control = value;
  record_event(MOCK_I2C_EVENT_CONTROL_WRITE, value);
}

void i2c0_write_status_clear(
  volatile i2c0_registers_t *i2c,
  uint32_t value
) {
  if (!is_i2c(i2c)) {
    state.invalid_access = true;
    return;
  }
  const uint32_t masked = value & I2C0_STATUS_ALL;
  if (state.status_clear_write_count < MOCK_I2C_HISTORY_CAPACITY) {
    state.status_clears[state.status_clear_write_count] = masked;
  } else {
    state.invalid_access = true;
  }
  state.status_clear_write_count++;
  state.status &= ~masked;
  record_event(MOCK_I2C_EVENT_STATUS_CLEAR, masked);
}

void i2c0_write_data(volatile i2c0_registers_t *i2c, uint8_t value) {
  if (!is_i2c(i2c)) {
    state.invalid_access = true;
    return;
  }
  if (state.data_write_count < MOCK_I2C_HISTORY_CAPACITY) {
    state.data[state.data_write_count] = value;
  } else {
    state.invalid_access = true;
  }
  state.data_write_count++;
  record_event(MOCK_I2C_EVENT_DATA_WRITE, value);
}

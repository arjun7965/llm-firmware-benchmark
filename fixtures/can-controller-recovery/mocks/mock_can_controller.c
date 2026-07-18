#include "mock_can_controller.h"

#define MOCK_CAN_HISTORY_CAPACITY 128u

struct can0_registers {
  uint32_t reserved;
};

typedef struct {
  mock_can_event_t event;
  uint32_t value;
} mock_can_event_record_t;

typedef struct {
  struct can0_registers can;
  uint32_t status;
  uint32_t control;
  uint32_t control_history[MOCK_CAN_HISTORY_CAPACITY];
  uint32_t bit_timing;
  uint32_t acceptance_filter;
  uint16_t tx_identifier;
  uint8_t tx_dlc;
  uint8_t tx_data[CAN0_MAX_DLC];
  uint16_t rx_identifier;
  uint8_t rx_dlc;
  uint8_t rx_data[CAN0_MAX_DLC];
  mock_can_event_record_t events[MOCK_CAN_HISTORY_CAPACITY];
  size_t event_count;
  size_t control_write_count;
  size_t bit_timing_write_count;
  size_t acceptance_filter_write_count;
  size_t tx_data_write_count;
  size_t rx_data_read_count;
  size_t status_read_count;
  size_t status_clear_write_count;
  size_t irq_save_count;
  size_t irq_restore_count;
  uint32_t last_status_clear;
  uint32_t last_irq_restore;
  bool rx_loaded;
  bool interrupts_enabled;
  bool invalid_access;
} mock_can_state_t;

static mock_can_state_t state;

static bool is_can(const volatile can0_registers_t *can) {
  return can == &state.can;
}

static void record_event(mock_can_event_t event, uint32_t value) {
  if (state.event_count < MOCK_CAN_HISTORY_CAPACITY) {
    state.events[state.event_count] = (mock_can_event_record_t) {
      .event = event,
      .value = value,
    };
  } else {
    state.invalid_access = true;
  }
  state.event_count++;
}

static bool tx_interrupt_is_enabled(void) {
  const uint32_t required = CAN0_CONTROL_ACTIVE |
    CAN0_CONTROL_TX_IRQ_ENABLE;
  return (state.control & required) == required;
}

void mock_can_reset(void) {
  state = (mock_can_state_t){ 0 };
  state.interrupts_enabled = true;
}

volatile can0_registers_t *mock_can0(void) {
  return &state.can;
}

bool mock_can_signal_tx_complete(void) {
  if (!tx_interrupt_is_enabled()) return false;
  state.status |= CAN0_STATUS_TX_COMPLETE;
  return true;
}

bool mock_can_signal_tx_error(void) {
  if (!tx_interrupt_is_enabled()) return false;
  state.status |= CAN0_STATUS_TX_ERROR;
  return true;
}

bool mock_can_inject_rx(
  uint16_t identifier,
  uint8_t dlc,
  const uint8_t *data
) {
  if (
    identifier > CAN0_STANDARD_ID_MAX || dlc > CAN0_MAX_DLC ||
    (dlc != 0u && data == NULL) || state.rx_loaded ||
    (state.control & CAN0_CONTROL_ACTIVE) != CAN0_CONTROL_ACTIVE
  ) {
    return false;
  }

  state.rx_identifier = identifier;
  state.rx_dlc = dlc;
  for (uint8_t index = 0u; index < dlc; index++) {
    state.rx_data[index] = data[index];
  }
  state.rx_loaded = true;
  state.rx_data_read_count = 0u;
  state.status |= CAN0_STATUS_RX_PENDING;
  return true;
}

void mock_can_set_status(uint32_t status) {
  state.status = status & CAN0_STATUS_ALL;
}

void mock_can_set_recovery_ready(bool ready) {
  if (ready) {
    state.status |= CAN0_STATUS_RECOVERY_READY;
  } else {
    state.status &= ~CAN0_STATUS_RECOVERY_READY;
  }
}

size_t mock_can_event_count(void) {
  return state.event_count;
}

mock_can_event_t mock_can_event_at(size_t index) {
  return index < state.event_count && index < MOCK_CAN_HISTORY_CAPACITY
    ? state.events[index].event
    : MOCK_CAN_EVENT_COUNT;
}

uint32_t mock_can_event_value(size_t index) {
  return index < state.event_count && index < MOCK_CAN_HISTORY_CAPACITY
    ? state.events[index].value
    : UINT32_MAX;
}

uint32_t mock_can_status(void) {
  return state.status;
}

size_t mock_can_status_read_count(void) {
  return state.status_read_count;
}

size_t mock_can_status_clear_write_count(void) {
  return state.status_clear_write_count;
}

uint32_t mock_can_last_status_clear(void) {
  return state.last_status_clear;
}

uint32_t mock_can_control(void) {
  return state.control;
}

size_t mock_can_control_write_count(void) {
  return state.control_write_count;
}

uint32_t mock_can_control_at(size_t index) {
  return index < state.control_write_count && index < MOCK_CAN_HISTORY_CAPACITY
    ? state.control_history[index]
    : UINT32_MAX;
}

uint32_t mock_can_bit_timing(void) {
  return state.bit_timing;
}

size_t mock_can_bit_timing_write_count(void) {
  return state.bit_timing_write_count;
}

uint32_t mock_can_acceptance_filter(void) {
  return state.acceptance_filter;
}

size_t mock_can_acceptance_filter_write_count(void) {
  return state.acceptance_filter_write_count;
}

uint16_t mock_can_tx_identifier(void) {
  return state.tx_identifier;
}

uint8_t mock_can_tx_dlc(void) {
  return state.tx_dlc;
}

size_t mock_can_tx_data_write_count(void) {
  return state.tx_data_write_count;
}

uint8_t mock_can_tx_data_at(size_t index) {
  return index < state.tx_data_write_count && index < CAN0_MAX_DLC
    ? state.tx_data[index]
    : UINT8_MAX;
}

void mock_can_set_interrupts_enabled(bool enabled) {
  state.interrupts_enabled = enabled;
}

bool mock_can_interrupts_enabled(void) {
  return state.interrupts_enabled;
}

size_t mock_can_irq_save_count(void) {
  return state.irq_save_count;
}

size_t mock_can_irq_restore_count(void) {
  return state.irq_restore_count;
}

uint32_t mock_can_last_irq_restore(void) {
  return state.last_irq_restore;
}

bool mock_can_invalid_access(void) {
  return state.invalid_access;
}

uint32_t can0_read_status(const volatile can0_registers_t *can) {
  if (!is_can(can)) {
    state.invalid_access = true;
    return 0u;
  }
  state.status_read_count++;
  record_event(MOCK_CAN_EVENT_STATUS_READ, state.status);
  return state.status;
}

void can0_write_status_clear(volatile can0_registers_t *can, uint32_t value) {
  if (!is_can(can)) {
    state.invalid_access = true;
    return;
  }
  const uint32_t masked = value & CAN0_STATUS_EVENT_MASK;
  state.status_clear_write_count++;
  state.last_status_clear = masked;
  state.status &= ~masked;
  record_event(MOCK_CAN_EVENT_STATUS_CLEAR, masked);
}

void can0_write_control(volatile can0_registers_t *can, uint32_t value) {
  if (!is_can(can)) {
    state.invalid_access = true;
    return;
  }
  if (state.control_write_count < MOCK_CAN_HISTORY_CAPACITY) {
    state.control_history[state.control_write_count] = value;
  } else {
    state.invalid_access = true;
  }
  state.control_write_count++;
  state.control = value;
  if (value == 0u) state.rx_loaded = false;
  record_event(MOCK_CAN_EVENT_CONTROL_WRITE, value);
}

void can0_write_bit_timing(volatile can0_registers_t *can, uint32_t value) {
  if (!is_can(can)) {
    state.invalid_access = true;
    return;
  }
  state.bit_timing = value;
  state.bit_timing_write_count++;
  record_event(MOCK_CAN_EVENT_BIT_TIMING_WRITE, value);
}

void can0_write_acceptance_filter(
  volatile can0_registers_t *can,
  uint32_t value
) {
  if (!is_can(can)) {
    state.invalid_access = true;
    return;
  }
  state.acceptance_filter = value;
  state.acceptance_filter_write_count++;
  record_event(MOCK_CAN_EVENT_ACCEPTANCE_FILTER_WRITE, value);
}

void can0_write_tx_identifier(
  volatile can0_registers_t *can,
  uint16_t value
) {
  if (!is_can(can)) {
    state.invalid_access = true;
    return;
  }
  state.tx_identifier = value;
  record_event(MOCK_CAN_EVENT_TX_IDENTIFIER_WRITE, value);
}

void can0_write_tx_dlc(volatile can0_registers_t *can, uint8_t value) {
  if (!is_can(can)) {
    state.invalid_access = true;
    return;
  }
  state.tx_dlc = value;
  state.tx_data_write_count = 0u;
  record_event(MOCK_CAN_EVENT_TX_DLC_WRITE, value);
}

void can0_write_tx_data(volatile can0_registers_t *can, uint8_t value) {
  if (!is_can(can)) {
    state.invalid_access = true;
    return;
  }
  if (state.tx_data_write_count < CAN0_MAX_DLC) {
    state.tx_data[state.tx_data_write_count] = value;
  } else {
    state.invalid_access = true;
  }
  state.tx_data_write_count++;
  record_event(MOCK_CAN_EVENT_TX_DATA_WRITE, value);
}

uint16_t can0_read_rx_identifier(volatile can0_registers_t *can) {
  if (!is_can(can) || !state.rx_loaded) {
    state.invalid_access = true;
    return 0u;
  }
  record_event(MOCK_CAN_EVENT_RX_IDENTIFIER_READ, state.rx_identifier);
  return state.rx_identifier;
}

uint8_t can0_read_rx_dlc(volatile can0_registers_t *can) {
  if (!is_can(can) || !state.rx_loaded) {
    state.invalid_access = true;
    return 0u;
  }
  record_event(MOCK_CAN_EVENT_RX_DLC_READ, state.rx_dlc);
  if (state.rx_dlc == 0u) state.rx_loaded = false;
  return state.rx_dlc;
}

uint8_t can0_read_rx_data(volatile can0_registers_t *can) {
  if (
    !is_can(can) || !state.rx_loaded ||
    state.rx_data_read_count >= state.rx_dlc
  ) {
    state.invalid_access = true;
    return 0u;
  }
  const uint8_t value = state.rx_data[state.rx_data_read_count];
  state.rx_data_read_count++;
  if (state.rx_data_read_count == state.rx_dlc) state.rx_loaded = false;
  record_event(MOCK_CAN_EVENT_RX_DATA_READ, value);
  return value;
}

uint32_t can0_irq_save_disable(void) {
  const uint32_t previous = state.interrupts_enabled ? UINT32_C(1) : 0u;
  state.interrupts_enabled = false;
  state.irq_save_count++;
  return previous;
}

void can0_irq_restore(uint32_t restore_state) {
  state.last_irq_restore = restore_state;
  state.interrupts_enabled = restore_state != 0u;
  state.irq_restore_count++;
}

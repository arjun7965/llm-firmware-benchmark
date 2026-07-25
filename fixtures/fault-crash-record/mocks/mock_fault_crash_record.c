#include "mock_fault_crash_record.h"

#define MOCK_FAULT_HISTORY_CAPACITY 128u

struct fault0_registers {
  uint32_t reserved;
};

typedef struct {
  mock_fault_event_t event;
  uint32_t value;
} mock_fault_event_record_t;

typedef struct {
  struct fault0_registers fault;
  uint32_t status;
  uint32_t control;
  uint32_t irq_state;
  mock_fault_event_record_t events[MOCK_FAULT_HISTORY_CAPACITY];
  size_t event_count;
  bool invalid_access;
} mock_fault_state_t;

static mock_fault_state_t state;

static bool is_fault(const volatile fault0_registers_t *fault) {
  return fault == &state.fault;
}

static void record_event(mock_fault_event_t event, uint32_t value) {
  if (state.event_count < MOCK_FAULT_HISTORY_CAPACITY) {
    state.events[state.event_count] = (mock_fault_event_record_t) {
      .event = event,
      .value = value,
    };
  } else {
    state.invalid_access = true;
  }
  state.event_count++;
}

void mock_fault0_reset(void) {
  state = (mock_fault_state_t) {
    .control = FAULT0_CONTROL_SAFE,
    .irq_state = UINT32_C(1),
  };
}

volatile fault0_registers_t *mock_fault0(void) {
  return &state.fault;
}

void mock_fault0_set_status(uint32_t value) {
  if ((value & ~FAULT0_STATUS_ALL) != 0u) state.invalid_access = true;
  state.status = value & FAULT0_STATUS_ALL;
}

void mock_fault0_set_irq_state(uint32_t value) {
  state.irq_state = value;
}

uint32_t mock_fault0_status(void) {
  return state.status;
}

uint32_t mock_fault0_control(void) {
  return state.control;
}

uint32_t mock_fault0_irq_state(void) {
  return state.irq_state;
}

size_t mock_fault0_event_count(void) {
  return state.event_count;
}

mock_fault_event_t mock_fault0_event_at(size_t index) {
  return index < state.event_count && index < MOCK_FAULT_HISTORY_CAPACITY
    ? state.events[index].event
    : MOCK_FAULT_EVENT_COUNT;
}

uint32_t mock_fault0_event_value(size_t index) {
  return index < state.event_count && index < MOCK_FAULT_HISTORY_CAPACITY
    ? state.events[index].value
    : UINT32_MAX;
}

bool mock_fault0_invalid_access(void) {
  return state.invalid_access;
}

uint32_t fault0_read_status(const volatile fault0_registers_t *fault) {
  if (!is_fault(fault)) {
    state.invalid_access = true;
    return 0u;
  }
  record_event(MOCK_FAULT_EVENT_STATUS_READ, state.status);
  return state.status;
}

void fault0_write_status_clear(
  volatile fault0_registers_t *fault,
  uint32_t value
) {
  if (!is_fault(fault)) {
    state.invalid_access = true;
    return;
  }
  if ((value & ~FAULT0_STATUS_ALL) != 0u) state.invalid_access = true;
  state.status &= ~(value & FAULT0_STATUS_ALL);
  record_event(MOCK_FAULT_EVENT_STATUS_CLEAR_WRITE, value & FAULT0_STATUS_ALL);
}

void fault0_write_control(volatile fault0_registers_t *fault, uint32_t value) {
  if (!is_fault(fault)) {
    state.invalid_access = true;
    return;
  }
  if (value != FAULT0_CONTROL_SAFE && value != FAULT0_CONTROL_NORMAL) {
    state.invalid_access = true;
    return;
  }
  state.control = value;
  record_event(MOCK_FAULT_EVENT_CONTROL_WRITE, value);
}

uint32_t fault0_irq_save_disable(void) {
  const uint32_t previous_state = state.irq_state;

  state.irq_state = 0u;
  record_event(MOCK_FAULT_EVENT_IRQ_SAVE_DISABLE, previous_state);
  return previous_state;
}

void fault0_irq_restore(uint32_t irq_state) {
  state.irq_state = irq_state;
  record_event(MOCK_FAULT_EVENT_IRQ_RESTORE, irq_state);
}

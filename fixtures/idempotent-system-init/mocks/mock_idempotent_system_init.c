#include "mock_idempotent_system_init.h"

#define MOCK_SYSTEM_HISTORY_CAPACITY 160u

struct system0_registers {
  uint32_t reserved;
};

typedef struct {
  mock_system_event_t event;
  uint32_t value;
} mock_system_event_record_t;

typedef struct {
  struct system0_registers system;
  uint32_t control;
  uint32_t clock_hz;
  uint32_t peripheral_mask;
  uint32_t irq_state;
  mock_system_event_record_t events[MOCK_SYSTEM_HISTORY_CAPACITY];
  size_t event_count;
  bool invalid_access;
} mock_system_state_t;

static mock_system_state_t state;

static bool is_system(const volatile system0_registers_t *system) {
  return system == &state.system;
}

static void record_event(mock_system_event_t event, uint32_t value) {
  if (state.event_count < MOCK_SYSTEM_HISTORY_CAPACITY) {
    state.events[state.event_count] = (mock_system_event_record_t) {
      .event = event,
      .value = value,
    };
  } else {
    state.invalid_access = true;
  }
  state.event_count++;
}

static bool valid_clock(uint32_t value) {
  return value >= SYSTEM0_MIN_CLOCK_HZ && value <= SYSTEM0_MAX_CLOCK_HZ &&
    value % SYSTEM0_CLOCK_STEP_HZ == 0u;
}

void mock_system0_reset(void) {
  state = (mock_system_state_t) {
    .control = SYSTEM0_CONTROL_SAFE,
    .irq_state = UINT32_C(1),
  };
}

volatile system0_registers_t *mock_system0(void) {
  return &state.system;
}

void mock_system0_set_irq_state(uint32_t value) {
  state.irq_state = value;
}

uint32_t mock_system0_control(void) {
  return state.control;
}

uint32_t mock_system0_clock_hz(void) {
  return state.clock_hz;
}

uint32_t mock_system0_peripheral_mask(void) {
  return state.peripheral_mask;
}

uint32_t mock_system0_irq_state(void) {
  return state.irq_state;
}

size_t mock_system0_event_count(void) {
  return state.event_count;
}

mock_system_event_t mock_system0_event_at(size_t index) {
  return index < state.event_count && index < MOCK_SYSTEM_HISTORY_CAPACITY
    ? state.events[index].event
    : MOCK_SYSTEM_EVENT_COUNT;
}

uint32_t mock_system0_event_value(size_t index) {
  return index < state.event_count && index < MOCK_SYSTEM_HISTORY_CAPACITY
    ? state.events[index].value
    : UINT32_MAX;
}

bool mock_system0_invalid_access(void) {
  return state.invalid_access;
}

void system0_write_control(volatile system0_registers_t *system, uint32_t value) {
  if (!is_system(system)) {
    state.invalid_access = true;
    return;
  }
  if (value != SYSTEM0_CONTROL_SAFE && value != SYSTEM0_CONTROL_READY) {
    state.invalid_access = true;
    return;
  }
  if (
    value == SYSTEM0_CONTROL_READY &&
    (!valid_clock(state.clock_hz) || state.peripheral_mask == 0u ||
      (state.peripheral_mask & ~SYSTEM0_PERIPHERAL_MASK_ALL) != 0u)
  ) {
    state.invalid_access = true;
  }
  state.control = value;
  record_event(MOCK_SYSTEM_EVENT_CONTROL_WRITE, value);
}

void system0_write_clock_hz(
  volatile system0_registers_t *system,
  uint32_t value
) {
  if (!is_system(system)) {
    state.invalid_access = true;
    return;
  }
  if (state.control != SYSTEM0_CONTROL_SAFE || !valid_clock(value)) {
    state.invalid_access = true;
  }
  state.clock_hz = value;
  record_event(MOCK_SYSTEM_EVENT_CLOCK_WRITE, value);
}

void system0_write_peripheral_mask(
  volatile system0_registers_t *system,
  uint32_t value
) {
  if (!is_system(system)) {
    state.invalid_access = true;
    return;
  }
  if (
    state.control != SYSTEM0_CONTROL_SAFE || value == 0u ||
    (value & ~SYSTEM0_PERIPHERAL_MASK_ALL) != 0u
  ) {
    state.invalid_access = true;
  }
  state.peripheral_mask = value;
  record_event(MOCK_SYSTEM_EVENT_PERIPHERAL_MASK_WRITE, value);
}

uint32_t system0_irq_save_disable(void) {
  const uint32_t previous_state = state.irq_state;

  state.irq_state = 0u;
  record_event(MOCK_SYSTEM_EVENT_IRQ_SAVE_DISABLE, previous_state);
  return previous_state;
}

void system0_irq_restore(uint32_t irq_state) {
  state.irq_state = irq_state;
  record_event(MOCK_SYSTEM_EVENT_IRQ_RESTORE, irq_state);
}

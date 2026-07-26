#include "mock_low_power_wake_clock.h"

#define MOCK_PWRCLK_HISTORY_CAPACITY 128u

struct pwrclk0_registers {
  uint32_t reserved;
};

typedef struct {
  mock_pwrclk_event_t event;
  uint32_t value;
} mock_pwrclk_event_record_t;

typedef struct {
  struct pwrclk0_registers registers;
  pwrclk0_clock_t clock;
  pwrclk0_sleep_t sleep_mode;
  uint32_t wake_enable;
  uint32_t wake_status;
  uint32_t irq_state;
  mock_pwrclk_event_record_t events[MOCK_PWRCLK_HISTORY_CAPACITY];
  size_t event_count;
  bool invalid_access;
} mock_pwrclk_state_t;

static mock_pwrclk_state_t state;

static bool is_registers(const volatile pwrclk0_registers_t *registers) {
  return registers == &state.registers;
}

static void record_event(mock_pwrclk_event_t event, uint32_t value) {
  if (state.event_count < MOCK_PWRCLK_HISTORY_CAPACITY) {
    state.events[state.event_count] = (mock_pwrclk_event_record_t) {
      .event = event,
      .value = value,
    };
  } else {
    state.invalid_access = true;
  }
  state.event_count++;
}

static uint32_t wake_masked(uint32_t value) {
  if ((value & ~PWRCLK0_WAKE_ALL) != 0u) state.invalid_access = true;
  return value & PWRCLK0_WAKE_ALL;
}

void mock_pwrclk_reset(void) {
  state = (mock_pwrclk_state_t) {
    .clock = PWRCLK0_CLOCK_RUN,
    .sleep_mode = PWRCLK0_SLEEP_NONE,
    .irq_state = UINT32_C(1),
  };
}

volatile pwrclk0_registers_t *mock_pwrclk0(void) {
  return &state.registers;
}

void mock_pwrclk_set_wake_status(uint32_t value) {
  state.wake_status = wake_masked(value);
}

void mock_pwrclk_raise_wake(uint32_t value) {
  state.wake_status |= wake_masked(value) & state.wake_enable;
}

void mock_pwrclk_set_irq_state(uint32_t value) {
  state.irq_state = value;
}

pwrclk0_clock_t mock_pwrclk_clock(void) {
  return state.clock;
}

pwrclk0_sleep_t mock_pwrclk_sleep_mode(void) {
  return state.sleep_mode;
}

uint32_t mock_pwrclk_wake_enable(void) {
  return state.wake_enable;
}

uint32_t mock_pwrclk_wake_status(void) {
  return state.wake_status;
}

uint32_t mock_pwrclk_irq_state(void) {
  return state.irq_state;
}

size_t mock_pwrclk_event_count(void) {
  return state.event_count;
}

mock_pwrclk_event_t mock_pwrclk_event_at(size_t index) {
  return index < state.event_count && index < MOCK_PWRCLK_HISTORY_CAPACITY
    ? state.events[index].event
    : MOCK_PWRCLK_EVENT_COUNT;
}

uint32_t mock_pwrclk_event_value(size_t index) {
  return index < state.event_count && index < MOCK_PWRCLK_HISTORY_CAPACITY
    ? state.events[index].value
    : UINT32_MAX;
}

bool mock_pwrclk_invalid_access(void) {
  return state.invalid_access;
}

void pwrclk0_write_clock(
  volatile pwrclk0_registers_t *registers,
  pwrclk0_clock_t value
) {
  if (!is_registers(registers) || value >= PWRCLK0_CLOCK_COUNT) {
    state.invalid_access = true;
    return;
  }
  state.clock = value;
  record_event(MOCK_PWRCLK_EVENT_CLOCK_WRITE, (uint32_t)value);
}

void pwrclk0_write_wake_enable(
  volatile pwrclk0_registers_t *registers,
  uint32_t value
) {
  if (!is_registers(registers)) {
    state.invalid_access = true;
    return;
  }
  state.wake_enable = wake_masked(value);
  record_event(MOCK_PWRCLK_EVENT_WAKE_ENABLE_WRITE, state.wake_enable);
}

uint32_t pwrclk0_read_wake_status(
  const volatile pwrclk0_registers_t *registers
) {
  if (!is_registers(registers)) {
    state.invalid_access = true;
    return 0u;
  }
  record_event(MOCK_PWRCLK_EVENT_WAKE_STATUS_READ, state.wake_status);
  return state.wake_status;
}

void pwrclk0_write_wake_clear(
  volatile pwrclk0_registers_t *registers,
  uint32_t value
) {
  uint32_t masked;

  if (!is_registers(registers)) {
    state.invalid_access = true;
    return;
  }
  masked = wake_masked(value);
  state.wake_status &= ~masked;
  record_event(MOCK_PWRCLK_EVENT_WAKE_CLEAR_WRITE, masked);
}

void pwrclk0_write_sleep_mode(
  volatile pwrclk0_registers_t *registers,
  pwrclk0_sleep_t value
) {
  if (!is_registers(registers) || value >= PWRCLK0_SLEEP_COUNT) {
    state.invalid_access = true;
    return;
  }
  state.sleep_mode = value;
  record_event(MOCK_PWRCLK_EVENT_SLEEP_MODE_WRITE, (uint32_t)value);
}

uint32_t pwrclk0_irq_save_disable(void) {
  const uint32_t previous = state.irq_state;

  state.irq_state = 0u;
  record_event(MOCK_PWRCLK_EVENT_IRQ_SAVE_DISABLE, previous);
  return previous;
}

void pwrclk0_irq_restore(uint32_t irq_state) {
  state.irq_state = irq_state;
  record_event(MOCK_PWRCLK_EVENT_IRQ_RESTORE, irq_state);
}

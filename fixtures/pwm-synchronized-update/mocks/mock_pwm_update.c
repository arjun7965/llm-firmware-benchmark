#include "mock_pwm_update.h"

#define MOCK_PWM_HISTORY_CAPACITY 192u

struct pwm0_registers {
  uint32_t reserved;
};

typedef struct {
  mock_pwm_event_t event;
  uint32_t value;
} mock_pwm_event_record_t;

typedef struct {
  struct pwm0_registers pwm;
  uint32_t status;
  uint32_t control;
  uint16_t period_shadow;
  uint16_t compare_shadow;
  uint16_t active_period;
  uint16_t active_duty;
  uint32_t load_pending;
  uint32_t irq_state;
  mock_pwm_event_record_t events[MOCK_PWM_HISTORY_CAPACITY];
  size_t event_count;
  bool invalid_access;
} mock_pwm_state_t;

static mock_pwm_state_t state;

static bool is_pwm(const volatile pwm0_registers_t *pwm) {
  return pwm == &state.pwm;
}

static void record_event(mock_pwm_event_t event, uint32_t value) {
  if (state.event_count < MOCK_PWM_HISTORY_CAPACITY) {
    state.events[state.event_count] = (mock_pwm_event_record_t) {
      .event = event,
      .value = value,
    };
  } else {
    state.invalid_access = true;
  }
  state.event_count++;
}

static uint32_t control_masked(uint32_t value) {
  if ((value & ~PWM0_CONTROL_ALL) != 0u) state.invalid_access = true;
  return value & PWM0_CONTROL_ALL;
}

static uint32_t status_masked(uint32_t value) {
  if ((value & ~PWM0_STATUS_ALL) != 0u) state.invalid_access = true;
  return value & PWM0_STATUS_ALL;
}

static uint32_t load_masked(uint32_t value) {
  if ((value & ~PWM0_LOAD_ALL) != 0u) state.invalid_access = true;
  return value & PWM0_LOAD_ALL;
}

static void latch_load(uint32_t load) {
  if ((load & PWM0_LOAD_PERIOD) != 0u) {
    state.active_period = state.period_shadow;
  }
  if ((load & PWM0_LOAD_COMPARE) != 0u) {
    state.active_duty = state.compare_shadow;
  }
}

void mock_pwm_reset(void) {
  state = (mock_pwm_state_t) {
    .irq_state = UINT32_C(1),
  };
}

volatile pwm0_registers_t *mock_pwm0(void) {
  return &state.pwm;
}

void mock_pwm_trigger_period_boundary(void) {
  if (
    (state.control & PWM0_CONTROL_ENABLE) == 0u ||
    state.load_pending == 0u
  ) {
    return;
  }

  latch_load(state.load_pending);
  state.load_pending = 0u;
  state.status |= PWM0_STATUS_UPDATE;
}

void mock_pwm_raise_fault(void) {
  state.status |= PWM0_STATUS_FAULT;
}

void mock_pwm_set_status(uint32_t value) {
  state.status = status_masked(value);
}

void mock_pwm_set_irq_state(uint32_t value) {
  state.irq_state = value;
}

uint32_t mock_pwm_status(void) {
  return state.status;
}

uint32_t mock_pwm_control(void) {
  return state.control;
}

uint16_t mock_pwm_period_shadow(void) {
  return state.period_shadow;
}

uint16_t mock_pwm_compare_shadow(void) {
  return state.compare_shadow;
}

uint16_t mock_pwm_active_period(void) {
  return state.active_period;
}

uint16_t mock_pwm_active_duty(void) {
  return state.active_duty;
}

uint32_t mock_pwm_load_pending(void) {
  return state.load_pending;
}

uint32_t mock_pwm_irq_state(void) {
  return state.irq_state;
}

size_t mock_pwm_event_count(void) {
  return state.event_count;
}

mock_pwm_event_t mock_pwm_event_at(size_t index) {
  return index < state.event_count && index < MOCK_PWM_HISTORY_CAPACITY
    ? state.events[index].event
    : MOCK_PWM_EVENT_COUNT;
}

uint32_t mock_pwm_event_value(size_t index) {
  return index < state.event_count && index < MOCK_PWM_HISTORY_CAPACITY
    ? state.events[index].value
    : UINT32_MAX;
}

bool mock_pwm_invalid_access(void) {
  return state.invalid_access;
}

uint32_t pwm0_read_status(const volatile pwm0_registers_t *pwm) {
  if (!is_pwm(pwm)) {
    state.invalid_access = true;
    return 0u;
  }
  record_event(MOCK_PWM_EVENT_STATUS_READ, state.status);
  return state.status;
}

void pwm0_write_control(volatile pwm0_registers_t *pwm, uint32_t value) {
  if (!is_pwm(pwm)) {
    state.invalid_access = true;
    return;
  }
  state.control = control_masked(value);
  record_event(MOCK_PWM_EVENT_CONTROL_WRITE, state.control);
}

void pwm0_write_period_shadow(
  volatile pwm0_registers_t *pwm,
  uint16_t value
) {
  if (!is_pwm(pwm)) {
    state.invalid_access = true;
    return;
  }
  if (value == 0u) state.invalid_access = true;
  state.period_shadow = value;
  record_event(MOCK_PWM_EVENT_PERIOD_SHADOW_WRITE, value);
}

void pwm0_write_compare_shadow(
  volatile pwm0_registers_t *pwm,
  uint16_t value
) {
  if (!is_pwm(pwm)) {
    state.invalid_access = true;
    return;
  }
  if (value > state.period_shadow) state.invalid_access = true;
  state.compare_shadow = value;
  record_event(MOCK_PWM_EVENT_COMPARE_SHADOW_WRITE, value);
}

void pwm0_write_load(volatile pwm0_registers_t *pwm, uint32_t value) {
  uint32_t masked;

  if (!is_pwm(pwm)) {
    state.invalid_access = true;
    return;
  }
  masked = load_masked(value);
  if ((state.control & PWM0_CONTROL_ENABLE) == 0u) {
    latch_load(masked);
    state.load_pending &= ~masked;
  } else {
    state.load_pending |= masked;
  }
  record_event(MOCK_PWM_EVENT_LOAD_WRITE, masked);
}

void pwm0_write_status_clear(
  volatile pwm0_registers_t *pwm,
  uint32_t value
) {
  uint32_t masked;

  if (!is_pwm(pwm)) {
    state.invalid_access = true;
    return;
  }
  masked = status_masked(value);
  state.status &= ~masked;
  record_event(MOCK_PWM_EVENT_STATUS_CLEAR_WRITE, masked);
}

uint32_t pwm0_irq_save_disable(void) {
  const uint32_t previous_state = state.irq_state;

  state.irq_state = 0u;
  record_event(MOCK_PWM_EVENT_IRQ_SAVE_DISABLE, previous_state);
  return previous_state;
}

void pwm0_irq_restore(uint32_t irq_state) {
  state.irq_state = irq_state;
  record_event(MOCK_PWM_EVENT_IRQ_RESTORE, irq_state);
}

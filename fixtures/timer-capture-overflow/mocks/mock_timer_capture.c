#include "mock_timer_capture.h"

#define MOCK_TIMER_CAPTURE_HISTORY_CAPACITY 256u

struct timer1_registers {
  uint32_t reserved;
};

typedef struct {
  mock_timer_capture_event_t event;
  uint32_t value;
} mock_timer_capture_event_record_t;

typedef struct {
  struct timer1_registers timer;
  uint32_t control;
  uint16_t count;
  uint16_t capture;
  uint16_t compare;
  uint32_t status;
  uint32_t irq_state;
  mock_timer_capture_event_record_t events[MOCK_TIMER_CAPTURE_HISTORY_CAPACITY];
  size_t event_count;
  bool invalid_access;
} mock_timer_capture_state_t;

static mock_timer_capture_state_t state;

static bool is_timer(const volatile timer1_registers_t *timer) {
  return timer == &state.timer;
}

static void record_event(mock_timer_capture_event_t event, uint32_t value) {
  if (state.event_count >= MOCK_TIMER_CAPTURE_HISTORY_CAPACITY) {
    state.invalid_access = true;
    return;
  }
  state.events[state.event_count++] = (mock_timer_capture_event_record_t) {
    .event = event,
    .value = value,
  };
}

void mock_timer_capture_reset(void) {
  state = (mock_timer_capture_state_t) {
    .irq_state = UINT32_C(1),
  };
}

volatile timer1_registers_t *mock_timer1(void) {
  return &state.timer;
}

void mock_timer_capture_set_count(uint16_t value) {
  state.count = value;
}

void mock_timer_capture_advance(uint32_t ticks) {
  for (uint32_t index = 0u; index < ticks; index++) {
    if ((state.control & TIMER1_CONTROL_ENABLE) == 0u) return;
    state.count = (uint16_t)(state.count + UINT16_C(1));
    if (state.count == 0u) state.status |= TIMER1_STATUS_OVERFLOW;
    if (
      (state.control & TIMER1_CONTROL_COMPARE_IRQ_ENABLE) != 0u &&
      state.count == state.compare
    ) {
      state.status |= TIMER1_STATUS_COMPARE;
    }
  }
}

void mock_timer_capture_latch_capture(uint16_t value) {
  if ((state.control & TIMER1_CONTROL_ENABLE) == 0u) return;
  state.capture = value;
  state.status |= TIMER1_STATUS_CAPTURE;
}

void mock_timer_capture_latch_status(uint32_t value) {
  if ((value & ~TIMER1_STATUS_ALL) != 0u) state.invalid_access = true;
  state.status |= value & TIMER1_STATUS_ALL;
}

void mock_timer_capture_set_irq_state(uint32_t value) {
  state.irq_state = value;
}

uint16_t timer1_read_count(const volatile timer1_registers_t *timer) {
  if (!is_timer(timer)) {
    state.invalid_access = true;
    return 0u;
  }
  record_event(MOCK_TIMER_CAPTURE_EVENT_COUNT_READ, state.count);
  return state.count;
}

uint16_t timer1_read_capture(const volatile timer1_registers_t *timer) {
  if (!is_timer(timer)) {
    state.invalid_access = true;
    return 0u;
  }
  record_event(MOCK_TIMER_CAPTURE_EVENT_CAPTURE_READ, state.capture);
  return state.capture;
}

uint32_t timer1_read_status(const volatile timer1_registers_t *timer) {
  if (!is_timer(timer)) {
    state.invalid_access = true;
    return 0u;
  }
  record_event(MOCK_TIMER_CAPTURE_EVENT_STATUS_READ, state.status);
  return state.status;
}

void timer1_write_control(volatile timer1_registers_t *timer, uint32_t value) {
  if (!is_timer(timer)) {
    state.invalid_access = true;
    return;
  }
  if (
    value != 0u && value != TIMER1_CONTROL_READY &&
    value != TIMER1_CONTROL_COMPARE_ARMED
  ) {
    state.invalid_access = true;
  }
  state.control = value & TIMER1_CONTROL_ALL;
  record_event(MOCK_TIMER_CAPTURE_EVENT_CONTROL_WRITE, state.control);
}

void timer1_write_compare(
  volatile timer1_registers_t *timer,
  uint16_t value
) {
  if (!is_timer(timer)) {
    state.invalid_access = true;
    return;
  }
  state.compare = value;
  record_event(MOCK_TIMER_CAPTURE_EVENT_COMPARE_WRITE, value);
}

void timer1_write_status_clear(
  volatile timer1_registers_t *timer,
  uint32_t value
) {
  const uint32_t cleared = value & TIMER1_STATUS_ALL;

  if (!is_timer(timer)) {
    state.invalid_access = true;
    return;
  }
  if ((value & ~TIMER1_STATUS_ALL) != 0u) state.invalid_access = true;
  state.status &= ~cleared;
  record_event(MOCK_TIMER_CAPTURE_EVENT_STATUS_CLEAR_WRITE, cleared);
}

uint32_t timer_capture_irq_save_disable(void) {
  const uint32_t previous_state = state.irq_state;

  state.irq_state = 0u;
  record_event(MOCK_TIMER_CAPTURE_EVENT_IRQ_SAVE_DISABLE, previous_state);
  return previous_state;
}

void timer_capture_irq_restore(uint32_t irq_state) {
  state.irq_state = irq_state;
  record_event(MOCK_TIMER_CAPTURE_EVENT_IRQ_RESTORE, irq_state);
}

uint32_t mock_timer1_control(void) {
  return state.control;
}

uint16_t mock_timer1_count(void) {
  return state.count;
}

uint16_t mock_timer1_capture(void) {
  return state.capture;
}

uint16_t mock_timer1_compare(void) {
  return state.compare;
}

uint32_t mock_timer1_status(void) {
  return state.status;
}

uint32_t mock_timer_capture_irq_state(void) {
  return state.irq_state;
}

size_t mock_timer_capture_event_count(void) {
  return state.event_count;
}

mock_timer_capture_event_t mock_timer_capture_event_at(size_t index) {
  if (index >= state.event_count) return MOCK_TIMER_CAPTURE_EVENT_COUNT;
  return state.events[index].event;
}

uint32_t mock_timer_capture_event_value(size_t index) {
  if (index >= state.event_count) return 0u;
  return state.events[index].value;
}

bool mock_timer_capture_invalid_access(void) {
  return state.invalid_access;
}

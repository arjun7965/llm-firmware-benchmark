#include "mock_watchdog_window.h"

#define MOCK_WDT_HISTORY_CAPACITY 192u

struct wdt0_registers {
  uint32_t reserved;
};

typedef struct {
  mock_wdt_event_t event;
  uint32_t value;
} mock_wdt_event_record_t;

typedef struct {
  struct wdt0_registers wdt;
  uint32_t status;
  uint32_t control;
  uint16_t timeout_ticks;
  uint16_t window_open_ticks;
  uint16_t counter_ticks;
  uint32_t reset_count;
  uint32_t irq_state;
  mock_wdt_event_record_t events[MOCK_WDT_HISTORY_CAPACITY];
  size_t event_count;
  bool invalid_access;
} mock_wdt_state_t;

static mock_wdt_state_t state;

static bool is_wdt(const volatile wdt0_registers_t *wdt) {
  return wdt == &state.wdt;
}

static void record_event(mock_wdt_event_t event, uint32_t value) {
  if (state.event_count < MOCK_WDT_HISTORY_CAPACITY) {
    state.events[state.event_count] = (mock_wdt_event_record_t) {
      .event = event,
      .value = value,
    };
  } else {
    state.invalid_access = true;
  }
  state.event_count++;
}

static uint32_t control_masked(uint32_t value) {
  if ((value & ~WDT0_CONTROL_ALL) != 0u) state.invalid_access = true;
  return value & WDT0_CONTROL_ALL;
}

static uint32_t status_masked(uint32_t value) {
  if ((value & ~WDT0_STATUS_ALL) != 0u) state.invalid_access = true;
  return value & WDT0_STATUS_ALL;
}

static void trigger_reset(void) {
  state.status |= WDT0_STATUS_RESET;
  state.control = 0u;
  state.counter_ticks = 0u;
  state.reset_count++;
}

void mock_wdt0_reset(void) {
  state = (mock_wdt_state_t) {
    .irq_state = UINT32_C(1),
  };
}

volatile wdt0_registers_t *mock_wdt0(void) {
  return &state.wdt;
}

void mock_wdt0_advance(uint16_t ticks) {
  uint16_t remaining;

  if ((state.control & WDT0_CONTROL_ENABLE) == 0u) return;
  if (state.timeout_ticks < UINT16_C(2)) {
    state.invalid_access = true;
    return;
  }

  remaining = (uint16_t)(state.timeout_ticks - state.counter_ticks);
  if (ticks >= remaining) {
    trigger_reset();
  } else {
    state.counter_ticks = (uint16_t)(state.counter_ticks + ticks);
  }
}

void mock_wdt0_trigger_reset(void) {
  trigger_reset();
}

void mock_wdt0_set_status(uint32_t value) {
  state.status = status_masked(value);
}

void mock_wdt0_set_irq_state(uint32_t value) {
  state.irq_state = value;
}

uint32_t mock_wdt0_status(void) {
  return state.status;
}

uint32_t mock_wdt0_control(void) {
  return state.control;
}

uint16_t mock_wdt0_timeout(void) {
  return state.timeout_ticks;
}

uint16_t mock_wdt0_window_open(void) {
  return state.window_open_ticks;
}

uint16_t mock_wdt0_counter(void) {
  return state.counter_ticks;
}

uint32_t mock_wdt0_reset_count(void) {
  return state.reset_count;
}

uint32_t mock_wdt0_irq_state(void) {
  return state.irq_state;
}

size_t mock_wdt0_event_count(void) {
  return state.event_count;
}

mock_wdt_event_t mock_wdt0_event_at(size_t index) {
  return index < state.event_count && index < MOCK_WDT_HISTORY_CAPACITY
    ? state.events[index].event
    : MOCK_WDT_EVENT_COUNT;
}

uint32_t mock_wdt0_event_value(size_t index) {
  return index < state.event_count && index < MOCK_WDT_HISTORY_CAPACITY
    ? state.events[index].value
    : UINT32_MAX;
}

bool mock_wdt0_invalid_access(void) {
  return state.invalid_access;
}

uint32_t wdt0_read_status(const volatile wdt0_registers_t *wdt) {
  if (!is_wdt(wdt)) {
    state.invalid_access = true;
    return 0u;
  }
  record_event(MOCK_WDT_EVENT_STATUS_READ, state.status);
  return state.status;
}

uint16_t wdt0_read_counter(const volatile wdt0_registers_t *wdt) {
  if (!is_wdt(wdt)) {
    state.invalid_access = true;
    return 0u;
  }
  record_event(MOCK_WDT_EVENT_COUNTER_READ, state.counter_ticks);
  return state.counter_ticks;
}

void wdt0_write_control(volatile wdt0_registers_t *wdt, uint32_t value) {
  const uint32_t masked = control_masked(value);

  if (!is_wdt(wdt)) {
    state.invalid_access = true;
    return;
  }
  if (
    masked == WDT0_CONTROL_READY &&
    (state.timeout_ticks < UINT16_C(2) ||
      state.window_open_ticks == 0u ||
      state.window_open_ticks >= state.timeout_ticks)
  ) {
    state.invalid_access = true;
  }
  state.control = masked;
  if (masked == 0u) state.counter_ticks = 0u;
  record_event(MOCK_WDT_EVENT_CONTROL_WRITE, masked);
}

void wdt0_write_timeout(volatile wdt0_registers_t *wdt, uint16_t value) {
  if (!is_wdt(wdt)) {
    state.invalid_access = true;
    return;
  }
  if (
    state.control != 0u || value < UINT16_C(2) ||
    value > WDT0_MAX_TIMEOUT_TICKS
  ) {
    state.invalid_access = true;
  }
  state.timeout_ticks = value;
  record_event(MOCK_WDT_EVENT_TIMEOUT_WRITE, value);
}

void wdt0_write_window_open(
  volatile wdt0_registers_t *wdt,
  uint16_t value
) {
  if (!is_wdt(wdt)) {
    state.invalid_access = true;
    return;
  }
  if (
    state.control != 0u || value == 0u ||
    (state.timeout_ticks != 0u && value >= state.timeout_ticks)
  ) {
    state.invalid_access = true;
  }
  state.window_open_ticks = value;
  record_event(MOCK_WDT_EVENT_WINDOW_OPEN_WRITE, value);
}

void wdt0_write_feed(volatile wdt0_registers_t *wdt, uint32_t value) {
  if (!is_wdt(wdt)) {
    state.invalid_access = true;
    return;
  }
  record_event(MOCK_WDT_EVENT_FEED_WRITE, value);
  if (state.control != WDT0_CONTROL_READY || value != WDT0_FEED_KEY) {
    state.invalid_access = true;
    return;
  }
  if (
    state.counter_ticks < state.window_open_ticks ||
    state.counter_ticks >= state.timeout_ticks
  ) {
    trigger_reset();
    return;
  }
  state.counter_ticks = 0u;
}

void wdt0_write_status_clear(
  volatile wdt0_registers_t *wdt,
  uint32_t value
) {
  const uint32_t masked = status_masked(value);

  if (!is_wdt(wdt)) {
    state.invalid_access = true;
    return;
  }
  state.status &= ~masked;
  record_event(MOCK_WDT_EVENT_STATUS_CLEAR_WRITE, masked);
}

uint32_t wdt0_irq_save_disable(void) {
  const uint32_t previous_state = state.irq_state;

  state.irq_state = 0u;
  record_event(MOCK_WDT_EVENT_IRQ_SAVE_DISABLE, previous_state);
  return previous_state;
}

void wdt0_irq_restore(uint32_t irq_state) {
  state.irq_state = irq_state;
  record_event(MOCK_WDT_EVENT_IRQ_RESTORE, irq_state);
}

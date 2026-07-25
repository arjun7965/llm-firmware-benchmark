#include "mock_brownout_safe_mode.h"

#define MOCK_PWR_HISTORY_CAPACITY 128u

struct pwr0_registers {
  uint32_t reserved;
};

typedef struct {
  mock_pwr_event_t event;
  uint32_t value;
} mock_pwr_event_record_t;

typedef struct {
  struct pwr0_registers pwr;
  uint32_t status;
  uint16_t supply_mv;
  uint32_t load_control;
  uint32_t irq_state;
  mock_pwr_event_record_t events[MOCK_PWR_HISTORY_CAPACITY];
  size_t event_count;
  bool invalid_access;
} mock_pwr_state_t;

static mock_pwr_state_t state;

static bool is_pwr(const volatile pwr0_registers_t *pwr) {
  return pwr == &state.pwr;
}

static void record_event(mock_pwr_event_t event, uint32_t value) {
  if (state.event_count < MOCK_PWR_HISTORY_CAPACITY) {
    state.events[state.event_count] = (mock_pwr_event_record_t) {
      .event = event,
      .value = value,
    };
  } else {
    state.invalid_access = true;
  }
  state.event_count++;
}

void mock_pwr0_reset(void) {
  state = (mock_pwr_state_t) {
    .supply_mv = BROWNOUT_MAXIMUM_MV,
    .load_control = PWR0_LOAD_SAFE,
    .irq_state = UINT32_C(1),
  };
}

volatile pwr0_registers_t *mock_pwr0(void) {
  return &state.pwr;
}

void mock_pwr0_set_status(uint32_t value) {
  if ((value & ~PWR0_STATUS_ALL) != 0u) state.invalid_access = true;
  state.status = value & PWR0_STATUS_ALL;
}

void mock_pwr0_set_supply_mv(uint16_t value) {
  state.supply_mv = value;
}

void mock_pwr0_set_irq_state(uint32_t value) {
  state.irq_state = value;
}

uint32_t mock_pwr0_status(void) {
  return state.status;
}

uint16_t mock_pwr0_supply_mv(void) {
  return state.supply_mv;
}

uint32_t mock_pwr0_load_control(void) {
  return state.load_control;
}

uint32_t mock_pwr0_irq_state(void) {
  return state.irq_state;
}

size_t mock_pwr0_event_count(void) {
  return state.event_count;
}

mock_pwr_event_t mock_pwr0_event_at(size_t index) {
  return index < state.event_count && index < MOCK_PWR_HISTORY_CAPACITY
    ? state.events[index].event
    : MOCK_PWR_EVENT_COUNT;
}

uint32_t mock_pwr0_event_value(size_t index) {
  return index < state.event_count && index < MOCK_PWR_HISTORY_CAPACITY
    ? state.events[index].value
    : UINT32_MAX;
}

bool mock_pwr0_invalid_access(void) {
  return state.invalid_access;
}

uint32_t pwr0_read_status(const volatile pwr0_registers_t *pwr) {
  if (!is_pwr(pwr)) {
    state.invalid_access = true;
    return 0u;
  }
  record_event(MOCK_PWR_EVENT_STATUS_READ, state.status);
  return state.status;
}

uint16_t pwr0_read_supply_mv(const volatile pwr0_registers_t *pwr) {
  if (!is_pwr(pwr)) {
    state.invalid_access = true;
    return 0u;
  }
  record_event(MOCK_PWR_EVENT_SUPPLY_READ, state.supply_mv);
  return state.supply_mv;
}

void pwr0_write_status_clear(volatile pwr0_registers_t *pwr, uint32_t value) {
  if (!is_pwr(pwr)) {
    state.invalid_access = true;
    return;
  }
  if ((value & ~PWR0_STATUS_ALL) != 0u) state.invalid_access = true;
  state.status &= ~(value & PWR0_STATUS_ALL);
  record_event(MOCK_PWR_EVENT_STATUS_CLEAR_WRITE, value & PWR0_STATUS_ALL);
}

void pwr0_write_load_control(volatile pwr0_registers_t *pwr, uint32_t value) {
  if (!is_pwr(pwr)) {
    state.invalid_access = true;
    return;
  }
  if (value != PWR0_LOAD_SAFE && value != PWR0_LOAD_ENABLED) {
    state.invalid_access = true;
    return;
  }
  state.load_control = value;
  record_event(MOCK_PWR_EVENT_LOAD_WRITE, value);
}

uint32_t pwr0_irq_save_disable(void) {
  const uint32_t previous_state = state.irq_state;

  state.irq_state = 0u;
  record_event(MOCK_PWR_EVENT_IRQ_SAVE_DISABLE, previous_state);
  return previous_state;
}

void pwr0_irq_restore(uint32_t irq_state) {
  state.irq_state = irq_state;
  record_event(MOCK_PWR_EVENT_IRQ_RESTORE, irq_state);
}

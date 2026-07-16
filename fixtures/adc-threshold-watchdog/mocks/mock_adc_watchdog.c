#include "mock_adc_watchdog.h"

#define MOCK_ADC_HISTORY_CAPACITY 192u

struct adc0_registers {
  uint32_t reserved;
};

typedef struct {
  mock_adc_event_t event;
  uint32_t value;
} mock_adc_event_record_t;

typedef struct {
  struct adc0_registers adc;
  uint32_t status;
  uint16_t data;
  uint32_t control;
  uint16_t lower_threshold;
  uint16_t upper_threshold;
  uint32_t irq_state;
  mock_adc_event_record_t events[MOCK_ADC_HISTORY_CAPACITY];
  size_t event_count;
  bool invalid_access;
} mock_adc_state_t;

static mock_adc_state_t state;

static bool is_adc(const volatile adc0_registers_t *adc) {
  return adc == &state.adc;
}

static void record_event(mock_adc_event_t event, uint32_t value) {
  if (state.event_count < MOCK_ADC_HISTORY_CAPACITY) {
    state.events[state.event_count] = (mock_adc_event_record_t) {
      .event = event,
      .value = value,
    };
  } else {
    state.invalid_access = true;
  }
  state.event_count++;
}

static uint32_t control_masked(uint32_t value) {
  if ((value & ~ADC0_CONTROL_ALL) != 0u) state.invalid_access = true;
  return value & ADC0_CONTROL_ALL;
}

static uint32_t status_masked(uint32_t value) {
  if ((value & ~ADC0_STATUS_ALL) != 0u) state.invalid_access = true;
  return value & ADC0_STATUS_ALL;
}

void mock_adc_reset(void) {
  state = (mock_adc_state_t) {
    .irq_state = UINT32_C(1),
  };
}

volatile adc0_registers_t *mock_adc0(void) {
  return &state.adc;
}

void mock_adc_set_sample(uint16_t sample) {
  state.data = sample;
}

void mock_adc_set_status(uint32_t value) {
  state.status = value;
}

void mock_adc_complete_sample(uint16_t sample) {
  state.data = sample;
  state.status |= ADC0_STATUS_EOC;
  if (
    sample < state.lower_threshold ||
    sample > state.upper_threshold
  ) {
    state.status |= ADC0_STATUS_AWD;
  }
  state.control &= ~ADC0_CONTROL_START;
}

void mock_adc_raise_overrun(void) {
  state.status |= ADC0_STATUS_OVERRUN;
  state.control &= ~ADC0_CONTROL_START;
}

void mock_adc_set_irq_state(uint32_t value) {
  state.irq_state = value;
}

uint32_t mock_adc_status(void) {
  return state.status;
}

uint16_t mock_adc_data(void) {
  return state.data;
}

uint32_t mock_adc_control(void) {
  return state.control;
}

uint16_t mock_adc_lower_threshold(void) {
  return state.lower_threshold;
}

uint16_t mock_adc_upper_threshold(void) {
  return state.upper_threshold;
}

uint32_t mock_adc_irq_state(void) {
  return state.irq_state;
}

size_t mock_adc_event_count(void) {
  return state.event_count;
}

mock_adc_event_t mock_adc_event_at(size_t index) {
  return index < state.event_count && index < MOCK_ADC_HISTORY_CAPACITY
    ? state.events[index].event
    : MOCK_ADC_EVENT_COUNT;
}

uint32_t mock_adc_event_value(size_t index) {
  return index < state.event_count && index < MOCK_ADC_HISTORY_CAPACITY
    ? state.events[index].value
    : UINT32_MAX;
}

bool mock_adc_invalid_access(void) {
  return state.invalid_access;
}

uint32_t adc0_read_status(const volatile adc0_registers_t *adc) {
  if (!is_adc(adc)) {
    state.invalid_access = true;
    return 0u;
  }
  record_event(MOCK_ADC_EVENT_STATUS_READ, state.status);
  return state.status;
}

uint16_t adc0_read_data(const volatile adc0_registers_t *adc) {
  if (!is_adc(adc)) {
    state.invalid_access = true;
    return 0u;
  }
  record_event(MOCK_ADC_EVENT_DATA_READ, state.data);
  return state.data;
}

void adc0_write_control(volatile adc0_registers_t *adc, uint32_t value) {
  if (!is_adc(adc)) {
    state.invalid_access = true;
    return;
  }
  state.control = control_masked(value);
  record_event(MOCK_ADC_EVENT_CONTROL_WRITE, state.control);
}

void adc0_write_lower_threshold(
  volatile adc0_registers_t *adc,
  uint16_t value
) {
  if (!is_adc(adc)) {
    state.invalid_access = true;
    return;
  }
  if (value > ADC0_MAX_SAMPLE) state.invalid_access = true;
  state.lower_threshold = value;
  record_event(MOCK_ADC_EVENT_LOWER_THRESHOLD_WRITE, value);
}

void adc0_write_upper_threshold(
  volatile adc0_registers_t *adc,
  uint16_t value
) {
  if (!is_adc(adc)) {
    state.invalid_access = true;
    return;
  }
  if (value > ADC0_MAX_SAMPLE) state.invalid_access = true;
  state.upper_threshold = value;
  record_event(MOCK_ADC_EVENT_UPPER_THRESHOLD_WRITE, value);
}

void adc0_write_status_clear(
  volatile adc0_registers_t *adc,
  uint32_t value
) {
  uint32_t masked;

  if (!is_adc(adc)) {
    state.invalid_access = true;
    return;
  }
  masked = status_masked(value);
  state.status &= ~masked;
  record_event(MOCK_ADC_EVENT_STATUS_CLEAR_WRITE, masked);
}

uint32_t adc0_irq_save_disable(void) {
  const uint32_t previous_state = state.irq_state;

  state.irq_state = 0u;
  record_event(MOCK_ADC_EVENT_IRQ_SAVE_DISABLE, previous_state);
  return previous_state;
}

void adc0_irq_restore(uint32_t irq_state) {
  state.irq_state = irq_state;
  record_event(MOCK_ADC_EVENT_IRQ_RESTORE, irq_state);
}

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "adc_watchdog.h"
#include "mock_adc_watchdog.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
              __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

typedef struct {
  mock_adc_event_t event;
  uint32_t value;
} expected_event_t;

static bool initialize(
  adc_watchdog_t *driver,
  uint16_t lower_threshold,
  uint16_t upper_threshold
) {
  return adc_watchdog_init(
    driver,
    mock_adc0(),
    lower_threshold,
    upper_threshold
  );
}

static bool state_equals(
  const adc_watchdog_t *left,
  const adc_watchdog_t *right
) {
  return left->adc == right->adc &&
    left->lower_threshold == right->lower_threshold &&
    left->upper_threshold == right->upper_threshold &&
    left->last_sample == right->last_sample &&
    left->started_at_ms == right->started_at_ms &&
    left->event == right->event &&
    left->active == right->active &&
    left->faulted == right->faulted &&
    left->initialized == right->initialized;
}

static bool events_match_from(
  size_t offset,
  const expected_event_t *expected,
  size_t expected_count
) {
  if (mock_adc_event_count() != offset + expected_count) return false;
  for (size_t index = 0u; index < expected_count; index++) {
    if (mock_adc_event_at(offset + index) != expected[index].event ||
        mock_adc_event_value(offset + index) != expected[index].value) {
      return false;
    }
  }
  return true;
}

static bool test_initialization_validation_order_and_reinitialization(void) {
  adc_watchdog_t driver = {
    .adc = (volatile adc0_registers_t *)(uintptr_t)UINT32_C(1),
    .lower_threshold = UINT16_C(1),
    .upper_threshold = UINT16_C(2),
    .last_sample = UINT16_C(3),
    .started_at_ms = UINT32_C(4),
    .event = ADC_WATCHDOG_EVENT_FAULT_TIMEOUT,
    .active = true,
    .faulted = true,
    .initialized = true,
  };
  const adc_watchdog_t before = driver;
  const expected_event_t initialization_events[] = {
    { MOCK_ADC_EVENT_CONTROL_WRITE, 0u },
    { MOCK_ADC_EVENT_LOWER_THRESHOLD_WRITE, UINT16_C(100) },
    { MOCK_ADC_EVENT_UPPER_THRESHOLD_WRITE, UINT16_C(900) },
    { MOCK_ADC_EVENT_STATUS_CLEAR_WRITE, ADC0_STATUS_ALL },
    { MOCK_ADC_EVENT_CONTROL_WRITE, ADC0_CONTROL_READY },
  };
  size_t offset;

  mock_adc_reset();
  mock_adc_set_status(ADC0_STATUS_ALL);
  CHECK(!adc_watchdog_init(NULL, mock_adc0(), UINT16_C(100), UINT16_C(900)));
  CHECK(!adc_watchdog_init(&driver, NULL, UINT16_C(100), UINT16_C(900)));
  CHECK(!adc_watchdog_init(&driver, mock_adc0(), UINT16_C(901), UINT16_C(900)));
  CHECK(!adc_watchdog_init(
    &driver,
    mock_adc0(),
    0u,
    UINT16_C(4096)
  ));
  CHECK(state_equals(&driver, &before));
  CHECK(mock_adc_event_count() == 0u);

  CHECK(initialize(&driver, UINT16_C(100), UINT16_C(900)));
  CHECK(events_match_from(
    0u,
    initialization_events,
    sizeof(initialization_events) / sizeof(initialization_events[0])
  ));
  CHECK(driver.adc == mock_adc0());
  CHECK(driver.lower_threshold == UINT16_C(100));
  CHECK(driver.upper_threshold == UINT16_C(900));
  CHECK(driver.last_sample == 0u);
  CHECK(driver.started_at_ms == 0u);
  CHECK(driver.event == ADC_WATCHDOG_EVENT_NONE);
  CHECK(!driver.active);
  CHECK(!driver.faulted);
  CHECK(driver.initialized);
  CHECK(mock_adc_control() == ADC0_CONTROL_READY);
  CHECK(mock_adc_lower_threshold() == UINT16_C(100));
  CHECK(mock_adc_upper_threshold() == UINT16_C(900));
  CHECK(mock_adc_status() == 0u);

  offset = mock_adc_event_count();
  CHECK(initialize(
    &driver,
    ADC_WATCHDOG_MAX_SAMPLE,
    ADC_WATCHDOG_MAX_SAMPLE
  ));
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_ADC_EVENT_CONTROL_WRITE, 0u },
      { MOCK_ADC_EVENT_LOWER_THRESHOLD_WRITE, ADC_WATCHDOG_MAX_SAMPLE },
      { MOCK_ADC_EVENT_UPPER_THRESHOLD_WRITE, ADC_WATCHDOG_MAX_SAMPLE },
      { MOCK_ADC_EVENT_STATUS_CLEAR_WRITE, ADC0_STATUS_ALL },
      { MOCK_ADC_EVENT_CONTROL_WRITE, ADC0_CONTROL_READY },
    },
    5u
  ));
  CHECK(driver.lower_threshold == ADC_WATCHDOG_MAX_SAMPLE);
  CHECK(driver.upper_threshold == ADC_WATCHDOG_MAX_SAMPLE);
  CHECK(!mock_adc_invalid_access());
  return true;
}

static bool test_start_is_atomic_and_rejects_busy_or_pending_state(void) {
  adc_watchdog_t driver = { 0 };
  adc_watchdog_t uninitialized = { 0 };
  adc_watchdog_t before;
  size_t offset;

  mock_adc_reset();
  CHECK(!adc_watchdog_start(NULL, 1u));
  CHECK(!adc_watchdog_start(&uninitialized, 1u));
  CHECK(mock_adc_event_count() == 0u);
  CHECK(initialize(&driver, UINT16_C(100), UINT16_C(900)));

  mock_adc_set_irq_state(UINT32_C(0xA5));
  offset = mock_adc_event_count();
  CHECK(adc_watchdog_start(&driver, UINT32_C(77)));
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_ADC_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xA5) },
      { MOCK_ADC_EVENT_STATUS_CLEAR_WRITE, ADC0_STATUS_ALL },
      { MOCK_ADC_EVENT_CONTROL_WRITE,
        ADC0_CONTROL_READY | ADC0_CONTROL_START },
      { MOCK_ADC_EVENT_IRQ_RESTORE, UINT32_C(0xA5) },
    },
    4u
  ));
  CHECK(driver.active);
  CHECK(!driver.faulted);
  CHECK(driver.started_at_ms == UINT32_C(77));
  CHECK(mock_adc_irq_state() == UINT32_C(0xA5));

  before = driver;
  offset = mock_adc_event_count();
  CHECK(!adc_watchdog_start(&driver, UINT32_C(78)));
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_ADC_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xA5) },
      { MOCK_ADC_EVENT_IRQ_RESTORE, UINT32_C(0xA5) },
    },
    2u
  ));
  CHECK(state_equals(&driver, &before));
  CHECK(!mock_adc_invalid_access());
  return true;
}

static bool test_in_window_conversion_and_event_consumption(void) {
  adc_watchdog_t driver = { 0 };
  size_t offset;

  mock_adc_reset();
  CHECK(initialize(&driver, UINT16_C(100), UINT16_C(900)));
  CHECK(adc_watchdog_start(&driver, UINT32_C(10)));
  mock_adc_complete_sample(UINT16_C(500));
  offset = mock_adc_event_count();
  adc_watchdog_irq(&driver);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_ADC_EVENT_STATUS_READ, ADC0_STATUS_EOC },
      { MOCK_ADC_EVENT_DATA_READ, UINT16_C(500) },
      { MOCK_ADC_EVENT_STATUS_CLEAR_WRITE, ADC0_STATUS_EOC },
    },
    3u
  ));
  CHECK(!driver.active);
  CHECK(!driver.faulted);
  CHECK(driver.last_sample == UINT16_C(500));
  CHECK(driver.event == ADC_WATCHDOG_EVENT_IN_WINDOW);
  CHECK(mock_adc_status() == 0u);

  mock_adc_set_irq_state(UINT32_C(0x4E));
  offset = mock_adc_event_count();
  CHECK(adc_watchdog_take_event(&driver) == ADC_WATCHDOG_EVENT_IN_WINDOW);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_ADC_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0x4E) },
      { MOCK_ADC_EVENT_IRQ_RESTORE, UINT32_C(0x4E) },
    },
    2u
  ));
  CHECK(driver.event == ADC_WATCHDOG_EVENT_NONE);
  CHECK(adc_watchdog_start(&driver, UINT32_C(11)));
  CHECK(!mock_adc_invalid_access());
  return true;
}

static bool test_watchdog_result_reads_once_and_wins_over_eoc(void) {
  adc_watchdog_t driver = { 0 };
  size_t offset;

  mock_adc_reset();
  CHECK(initialize(&driver, UINT16_C(100), UINT16_C(900)));
  CHECK(adc_watchdog_start(&driver, 1u));
  mock_adc_complete_sample(UINT16_C(901));
  offset = mock_adc_event_count();
  adc_watchdog_irq(&driver);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_ADC_EVENT_STATUS_READ, ADC0_STATUS_EOC | ADC0_STATUS_AWD },
      { MOCK_ADC_EVENT_DATA_READ, UINT16_C(901) },
      { MOCK_ADC_EVENT_STATUS_CLEAR_WRITE,
        ADC0_STATUS_EOC | ADC0_STATUS_AWD },
    },
    3u
  ));
  CHECK(driver.event == ADC_WATCHDOG_EVENT_OUT_OF_WINDOW);
  CHECK(driver.last_sample == UINT16_C(901));
  CHECK(!driver.active);
  CHECK(!driver.faulted);
  CHECK(adc_watchdog_take_event(&driver) == ADC_WATCHDOG_EVENT_OUT_OF_WINDOW);

  CHECK(adc_watchdog_start(&driver, 2u));
  mock_adc_set_sample(UINT16_C(99));
  mock_adc_set_status(ADC0_STATUS_AWD);
  offset = mock_adc_event_count();
  adc_watchdog_irq(&driver);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_ADC_EVENT_STATUS_READ, ADC0_STATUS_AWD },
      { MOCK_ADC_EVENT_DATA_READ, UINT16_C(99) },
      { MOCK_ADC_EVENT_STATUS_CLEAR_WRITE, ADC0_STATUS_AWD },
    },
    3u
  ));
  CHECK(driver.event == ADC_WATCHDOG_EVENT_OUT_OF_WINDOW);
  CHECK(driver.last_sample == UINT16_C(99));
  CHECK(!mock_adc_invalid_access());
  return true;
}

static bool test_overrun_fault_requires_recovery_and_event_take(void) {
  adc_watchdog_t driver = { 0 };
  size_t offset;

  mock_adc_reset();
  CHECK(initialize(&driver, UINT16_C(100), UINT16_C(900)));
  CHECK(adc_watchdog_start(&driver, 1u));
  mock_adc_set_sample(UINT16_C(50));
  mock_adc_set_status(ADC0_STATUS_ALL);
  offset = mock_adc_event_count();
  adc_watchdog_irq(&driver);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_ADC_EVENT_STATUS_READ, ADC0_STATUS_ALL },
      { MOCK_ADC_EVENT_CONTROL_WRITE, 0u },
      { MOCK_ADC_EVENT_STATUS_CLEAR_WRITE, ADC0_STATUS_ALL },
    },
    3u
  ));
  CHECK(!driver.active);
  CHECK(driver.faulted);
  CHECK(driver.event == ADC_WATCHDOG_EVENT_FAULT_OVERRUN);
  CHECK(mock_adc_status() == 0u);

  mock_adc_set_irq_state(UINT32_C(0xC3));
  offset = mock_adc_event_count();
  CHECK(!adc_watchdog_start(&driver, 2u));
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_ADC_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xC3) },
      { MOCK_ADC_EVENT_IRQ_RESTORE, UINT32_C(0xC3) },
    },
    2u
  ));

  offset = mock_adc_event_count();
  CHECK(adc_watchdog_recover(&driver));
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_ADC_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xC3) },
      { MOCK_ADC_EVENT_CONTROL_WRITE, 0u },
      { MOCK_ADC_EVENT_LOWER_THRESHOLD_WRITE, UINT16_C(100) },
      { MOCK_ADC_EVENT_UPPER_THRESHOLD_WRITE, UINT16_C(900) },
      { MOCK_ADC_EVENT_STATUS_CLEAR_WRITE, ADC0_STATUS_ALL },
      { MOCK_ADC_EVENT_CONTROL_WRITE, ADC0_CONTROL_READY },
      { MOCK_ADC_EVENT_IRQ_RESTORE, UINT32_C(0xC3) },
    },
    7u
  ));
  CHECK(!driver.faulted);
  CHECK(driver.event == ADC_WATCHDOG_EVENT_FAULT_OVERRUN);

  offset = mock_adc_event_count();
  CHECK(!adc_watchdog_start(&driver, 3u));
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_ADC_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xC3) },
      { MOCK_ADC_EVENT_IRQ_RESTORE, UINT32_C(0xC3) },
    },
    2u
  ));
  CHECK(adc_watchdog_take_event(&driver) == ADC_WATCHDOG_EVENT_FAULT_OVERRUN);
  CHECK(adc_watchdog_start(&driver, 4u));
  CHECK(!mock_adc_invalid_access());
  return true;
}

static bool test_timeout_is_wrap_safe_and_wins_over_unserved_status(void) {
  adc_watchdog_t driver = { 0 };
  const uint32_t started_at = UINT32_MAX - UINT32_C(10);
  size_t offset;

  mock_adc_reset();
  CHECK(initialize(&driver, UINT16_C(100), UINT16_C(900)));
  CHECK(adc_watchdog_start(&driver, started_at));
  mock_adc_set_irq_state(UINT32_C(0x67));

  offset = mock_adc_event_count();
  CHECK(adc_watchdog_poll(&driver, UINT32_C(13)) ==
    ADC_WATCHDOG_EVENT_NONE);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_ADC_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0x67) },
      { MOCK_ADC_EVENT_IRQ_RESTORE, UINT32_C(0x67) },
    },
    2u
  ));
  CHECK(driver.active);

  mock_adc_set_status(ADC0_STATUS_EOC);
  offset = mock_adc_event_count();
  CHECK(adc_watchdog_poll(&driver, UINT32_C(14)) ==
    ADC_WATCHDOG_EVENT_FAULT_TIMEOUT);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_ADC_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0x67) },
      { MOCK_ADC_EVENT_CONTROL_WRITE, 0u },
      { MOCK_ADC_EVENT_STATUS_CLEAR_WRITE, ADC0_STATUS_ALL },
      { MOCK_ADC_EVENT_IRQ_RESTORE, UINT32_C(0x67) },
    },
    4u
  ));
  CHECK(!driver.active);
  CHECK(driver.faulted);
  CHECK(driver.event == ADC_WATCHDOG_EVENT_FAULT_TIMEOUT);
  CHECK(mock_adc_status() == 0u);
  CHECK(mock_adc_irq_state() == UINT32_C(0x67));
  CHECK(!mock_adc_invalid_access());
  return true;
}

static bool test_invalid_irq_unrelated_status_and_idle_foreground_calls(void) {
  adc_watchdog_t driver = { 0 };
  adc_watchdog_t uninitialized = { 0 };
  adc_watchdog_t before;
  size_t offset;

  mock_adc_reset();
  adc_watchdog_irq(NULL);
  adc_watchdog_irq(&uninitialized);
  CHECK(adc_watchdog_poll(NULL, 0u) == ADC_WATCHDOG_EVENT_NONE);
  CHECK(!adc_watchdog_recover(NULL));
  CHECK(adc_watchdog_take_event(NULL) == ADC_WATCHDOG_EVENT_NONE);
  CHECK(mock_adc_event_count() == 0u);
  CHECK(initialize(&driver, UINT16_C(100), UINT16_C(900)));

  mock_adc_set_irq_state(UINT32_C(0xB4));
  offset = mock_adc_event_count();
  CHECK(adc_watchdog_poll(&driver, 0u) == ADC_WATCHDOG_EVENT_NONE);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_ADC_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xB4) },
      { MOCK_ADC_EVENT_IRQ_RESTORE, UINT32_C(0xB4) },
    },
    2u
  ));
  offset = mock_adc_event_count();
  CHECK(!adc_watchdog_recover(&driver));
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_ADC_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xB4) },
      { MOCK_ADC_EVENT_IRQ_RESTORE, UINT32_C(0xB4) },
    },
    2u
  ));
  offset = mock_adc_event_count();
  CHECK(adc_watchdog_take_event(&driver) == ADC_WATCHDOG_EVENT_NONE);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_ADC_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xB4) },
      { MOCK_ADC_EVENT_IRQ_RESTORE, UINT32_C(0xB4) },
    },
    2u
  ));

  CHECK(adc_watchdog_start(&driver, 1u));
  mock_adc_set_status(UINT32_C(1) << 8);
  before = driver;
  offset = mock_adc_event_count();
  adc_watchdog_irq(&driver);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_ADC_EVENT_STATUS_READ, UINT32_C(1) << 8 },
    },
    1u
  ));
  CHECK(state_equals(&driver, &before));
  CHECK(!mock_adc_invalid_access());
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "initialization validation and ordering",
      test_initialization_validation_order_and_reinitialization },
    { "atomic start and busy rejection",
      test_start_is_atomic_and_rejects_busy_or_pending_state },
    { "in-window conversion and event take",
      test_in_window_conversion_and_event_consumption },
    { "watchdog result priority", test_watchdog_result_reads_once_and_wins_over_eoc },
    { "overrun recovery", test_overrun_fault_requires_recovery_and_event_take },
    { "wrap-safe timeout", test_timeout_is_wrap_safe_and_wins_over_unserved_status },
    { "invalid IRQ and idle foreground calls",
      test_invalid_irq_unrelated_status_and_idle_foreground_calls },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) {
      fprintf(stderr, "test failed: %s\n", tests[index].name);
      return 1;
    }
  }
  printf("ADC threshold/watchdog public tests passed.\n");
  return 0;
}

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "mock_timer_capture.h"
#include "timer_capture.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
        __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

typedef struct {
  mock_timer_capture_event_t event;
  uint32_t value;
} expected_event_t;

static bool initialize(timer_capture_t *driver) {
  return timer_capture_init(driver, mock_timer1());
}

static bool state_equals(
  const timer_capture_t *left,
  const timer_capture_t *right
) {
  return left->timer == right->timer &&
    left->overflow_ticks == right->overflow_ticks &&
    left->capture_timestamp == right->capture_timestamp &&
    left->compare_timestamp == right->compare_timestamp &&
    left->capture_overruns == right->capture_overruns &&
    left->compare_deadline == right->compare_deadline &&
    left->compare_ticks == right->compare_ticks &&
    left->capture_pending == right->capture_pending &&
    left->compare_pending == right->compare_pending &&
    left->compare_armed == right->compare_armed &&
    left->initialized == right->initialized;
}

static bool events_match_from(
  size_t offset,
  const expected_event_t *expected,
  size_t expected_count
) {
  if (mock_timer_capture_event_count() != offset + expected_count) {
    return false;
  }
  for (size_t index = 0u; index < expected_count; index++) {
    if (
      mock_timer_capture_event_at(offset + index) != expected[index].event ||
      mock_timer_capture_event_value(offset + index) != expected[index].value
    ) {
      return false;
    }
  }
  return true;
}

static bool test_initialization_validation_and_baseline(void) {
  timer_capture_t driver = {
    .timer = (volatile timer1_registers_t *)(uintptr_t)UINT32_C(1),
    .overflow_ticks = UINT32_C(0x10000),
    .capture_timestamp = UINT32_C(7),
    .compare_timestamp = UINT32_C(8),
    .capture_overruns = UINT32_C(9),
    .compare_deadline = UINT32_C(10),
    .compare_ticks = UINT16_C(11),
    .capture_pending = true,
    .compare_pending = true,
    .compare_armed = true,
    .initialized = true,
  };
  const timer_capture_t before = driver;
  const expected_event_t expected[] = {
    { MOCK_TIMER_CAPTURE_EVENT_CONTROL_WRITE, 0u },
    { MOCK_TIMER_CAPTURE_EVENT_STATUS_CLEAR_WRITE, TIMER1_STATUS_ALL },
    { MOCK_TIMER_CAPTURE_EVENT_COMPARE_WRITE, 0u },
    { MOCK_TIMER_CAPTURE_EVENT_CONTROL_WRITE, TIMER1_CONTROL_READY },
  };

  mock_timer_capture_reset();
  CHECK(!timer_capture_init(NULL, mock_timer1()));
  CHECK(!timer_capture_init(&driver, NULL));
  CHECK(state_equals(&driver, &before));
  CHECK(mock_timer_capture_event_count() == 0u);

  CHECK(initialize(&driver));
  CHECK(events_match_from(0u, expected, sizeof(expected) / sizeof(expected[0])));
  CHECK(driver.timer == mock_timer1());
  CHECK(driver.overflow_ticks == 0u);
  CHECK(driver.capture_timestamp == 0u);
  CHECK(driver.compare_timestamp == 0u);
  CHECK(driver.capture_overruns == 0u);
  CHECK(driver.compare_deadline == 0u);
  CHECK(driver.compare_ticks == 0u);
  CHECK(!driver.capture_pending);
  CHECK(!driver.compare_pending);
  CHECK(!driver.compare_armed);
  CHECK(driver.initialized);
  CHECK(mock_timer1_control() == TIMER1_CONTROL_READY);
  CHECK(mock_timer1_compare() == 0u);
  CHECK(mock_timer1_status() == 0u);
  CHECK(!mock_timer_capture_invalid_access());
  return true;
}

static bool test_capture_timestamps_recover_across_overflow_handoff(void) {
  timer_capture_t driver = { 0 };
  uint32_t timestamp = UINT32_MAX;
  const expected_event_t capture_events[] = {
    { MOCK_TIMER_CAPTURE_EVENT_STATUS_READ, TIMER1_STATUS_CAPTURE },
    { MOCK_TIMER_CAPTURE_EVENT_CAPTURE_READ, UINT16_C(0x1234) },
    { MOCK_TIMER_CAPTURE_EVENT_STATUS_CLEAR_WRITE, TIMER1_STATUS_CAPTURE },
  };
  const expected_event_t overflow_events[] = {
    { MOCK_TIMER_CAPTURE_EVENT_STATUS_READ, TIMER1_STATUS_OVERFLOW },
    { MOCK_TIMER_CAPTURE_EVENT_STATUS_CLEAR_WRITE, TIMER1_STATUS_OVERFLOW },
  };
  const expected_event_t before_wrap_events[] = {
    {
      MOCK_TIMER_CAPTURE_EVENT_STATUS_READ,
      TIMER1_STATUS_CAPTURE | TIMER1_STATUS_OVERFLOW,
    },
    { MOCK_TIMER_CAPTURE_EVENT_CAPTURE_READ, UINT16_C(0xfff0) },
    {
      MOCK_TIMER_CAPTURE_EVENT_STATUS_CLEAR_WRITE,
      TIMER1_STATUS_CAPTURE | TIMER1_STATUS_OVERFLOW,
    },
  };
  const expected_event_t after_wrap_events[] = {
    {
      MOCK_TIMER_CAPTURE_EVENT_STATUS_READ,
      TIMER1_STATUS_CAPTURE | TIMER1_STATUS_OVERFLOW,
    },
    { MOCK_TIMER_CAPTURE_EVENT_CAPTURE_READ, UINT16_C(0x0020) },
    {
      MOCK_TIMER_CAPTURE_EVENT_STATUS_CLEAR_WRITE,
      TIMER1_STATUS_CAPTURE | TIMER1_STATUS_OVERFLOW,
    },
  };
  size_t offset;

  mock_timer_capture_reset();
  CHECK(initialize(&driver));

  mock_timer_capture_latch_capture(UINT16_C(0x1234));
  offset = mock_timer_capture_event_count();
  timer_capture_irq(&driver);
  CHECK(events_match_from(
    offset,
    capture_events,
    sizeof(capture_events) / sizeof(capture_events[0])
  ));
  CHECK(driver.capture_pending);
  CHECK(driver.capture_timestamp == UINT32_C(0x1234));
  CHECK(timer_capture_take_capture(&driver, &timestamp));
  CHECK(timestamp == UINT32_C(0x1234));

  mock_timer_capture_latch_status(TIMER1_STATUS_OVERFLOW);
  offset = mock_timer_capture_event_count();
  timer_capture_irq(&driver);
  CHECK(events_match_from(
    offset,
    overflow_events,
    sizeof(overflow_events) / sizeof(overflow_events[0])
  ));
  CHECK(driver.overflow_ticks == UINT32_C(0x10000));

  mock_timer_capture_latch_capture(UINT16_C(0xfff0));
  mock_timer_capture_latch_status(TIMER1_STATUS_OVERFLOW);
  offset = mock_timer_capture_event_count();
  timer_capture_irq(&driver);
  CHECK(events_match_from(
    offset,
    before_wrap_events,
    sizeof(before_wrap_events) / sizeof(before_wrap_events[0])
  ));
  CHECK(driver.overflow_ticks == UINT32_C(0x20000));
  CHECK(timer_capture_take_capture(&driver, &timestamp));
  CHECK(timestamp == UINT32_C(0x1fff0));

  mock_timer_capture_latch_capture(UINT16_C(0x0020));
  mock_timer_capture_latch_status(TIMER1_STATUS_OVERFLOW);
  offset = mock_timer_capture_event_count();
  timer_capture_irq(&driver);
  CHECK(events_match_from(
    offset,
    after_wrap_events,
    sizeof(after_wrap_events) / sizeof(after_wrap_events[0])
  ));
  CHECK(driver.overflow_ticks == UINT32_C(0x30000));
  CHECK(timer_capture_take_capture(&driver, &timestamp));
  CHECK(timestamp == UINT32_C(0x30020));
  CHECK(!mock_timer_capture_invalid_access());
  return true;
}

static bool test_compare_arms_and_fires_once_across_counter_wrap(void) {
  timer_capture_t driver = { 0 };
  uint32_t timestamp = UINT32_MAX;
  const expected_event_t arm_events[] = {
    { MOCK_TIMER_CAPTURE_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xa5) },
    { MOCK_TIMER_CAPTURE_EVENT_STATUS_READ, 0u },
    { MOCK_TIMER_CAPTURE_EVENT_COUNT_READ, UINT16_C(0xfff0) },
    { MOCK_TIMER_CAPTURE_EVENT_COMPARE_WRITE, UINT16_C(0x0010) },
    {
      MOCK_TIMER_CAPTURE_EVENT_CONTROL_WRITE,
      TIMER1_CONTROL_COMPARE_ARMED,
    },
    { MOCK_TIMER_CAPTURE_EVENT_IRQ_RESTORE, UINT32_C(0xa5) },
  };
  const expected_event_t compare_irq_events[] = {
    {
      MOCK_TIMER_CAPTURE_EVENT_STATUS_READ,
      TIMER1_STATUS_OVERFLOW | TIMER1_STATUS_COMPARE,
    },
    {
      MOCK_TIMER_CAPTURE_EVENT_STATUS_CLEAR_WRITE,
      TIMER1_STATUS_OVERFLOW | TIMER1_STATUS_COMPARE,
    },
    { MOCK_TIMER_CAPTURE_EVENT_CONTROL_WRITE, TIMER1_CONTROL_READY },
  };
  const expected_event_t rejected_arm_events[] = {
    { MOCK_TIMER_CAPTURE_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xa5) },
    { MOCK_TIMER_CAPTURE_EVENT_IRQ_RESTORE, UINT32_C(0xa5) },
  };
  const expected_event_t take_events[] = {
    { MOCK_TIMER_CAPTURE_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xa5) },
    { MOCK_TIMER_CAPTURE_EVENT_IRQ_RESTORE, UINT32_C(0xa5) },
  };
  size_t offset;

  mock_timer_capture_reset();
  CHECK(initialize(&driver));
  mock_timer_capture_set_count(UINT16_C(0xfff0));
  mock_timer_capture_set_irq_state(UINT32_C(0xa5));

  offset = mock_timer_capture_event_count();
  CHECK(timer_capture_arm_compare(&driver, UINT16_C(0x20)));
  CHECK(events_match_from(
    offset,
    arm_events,
    sizeof(arm_events) / sizeof(arm_events[0])
  ));
  CHECK(driver.compare_armed);
  CHECK(driver.compare_ticks == UINT16_C(0x0010));
  CHECK(driver.compare_deadline == UINT32_C(0x10010));
  CHECK(mock_timer1_control() == TIMER1_CONTROL_COMPARE_ARMED);

  mock_timer_capture_advance(UINT32_C(0x20));
  CHECK(mock_timer1_count() == UINT16_C(0x0010));
  CHECK(mock_timer1_status() ==
    (TIMER1_STATUS_OVERFLOW | TIMER1_STATUS_COMPARE));
  offset = mock_timer_capture_event_count();
  timer_capture_irq(&driver);
  CHECK(events_match_from(
    offset,
    compare_irq_events,
    sizeof(compare_irq_events) / sizeof(compare_irq_events[0])
  ));
  CHECK(driver.overflow_ticks == UINT32_C(0x10000));
  CHECK(!driver.compare_armed);
  CHECK(driver.compare_pending);
  CHECK(mock_timer1_control() == TIMER1_CONTROL_READY);

  offset = mock_timer_capture_event_count();
  CHECK(!timer_capture_arm_compare(&driver, UINT16_C(1)));
  CHECK(events_match_from(
    offset,
    rejected_arm_events,
    sizeof(rejected_arm_events) / sizeof(rejected_arm_events[0])
  ));

  offset = mock_timer_capture_event_count();
  CHECK(timer_capture_take_compare(&driver, &timestamp));
  CHECK(events_match_from(
    offset,
    take_events,
    sizeof(take_events) / sizeof(take_events[0])
  ));
  CHECK(timestamp == UINT32_C(0x10010));
  CHECK(!driver.compare_pending);

  CHECK(!timer_capture_arm_compare(
    &driver,
    (uint16_t)(TIMER_CAPTURE_MAX_DELAY + UINT16_C(1))
  ));
  mock_timer_capture_set_count(UINT16_C(2));
  CHECK(timer_capture_arm_compare(&driver, TIMER_CAPTURE_MAX_DELAY));
  CHECK(driver.compare_deadline == UINT32_C(0x18001));
  CHECK(!mock_timer_capture_invalid_access());
  return true;
}

static bool test_pending_status_blocks_compare_and_stale_compare_is_ignored(void) {
  timer_capture_t driver = { 0 };
  uint32_t timestamp = UINT32_MAX;
  const expected_event_t rejected_events[] = {
    { MOCK_TIMER_CAPTURE_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0x3c) },
    { MOCK_TIMER_CAPTURE_EVENT_STATUS_READ, TIMER1_STATUS_OVERFLOW },
    { MOCK_TIMER_CAPTURE_EVENT_IRQ_RESTORE, UINT32_C(0x3c) },
  };
  const expected_event_t overflow_events[] = {
    { MOCK_TIMER_CAPTURE_EVENT_STATUS_READ, TIMER1_STATUS_OVERFLOW },
    { MOCK_TIMER_CAPTURE_EVENT_STATUS_CLEAR_WRITE, TIMER1_STATUS_OVERFLOW },
  };
  const expected_event_t stale_compare_events[] = {
    { MOCK_TIMER_CAPTURE_EVENT_STATUS_READ, TIMER1_STATUS_COMPARE },
    { MOCK_TIMER_CAPTURE_EVENT_STATUS_CLEAR_WRITE, TIMER1_STATUS_COMPARE },
  };
  size_t offset;

  mock_timer_capture_reset();
  CHECK(initialize(&driver));
  mock_timer_capture_set_irq_state(UINT32_C(0x3c));
  mock_timer_capture_latch_status(TIMER1_STATUS_OVERFLOW);

  offset = mock_timer_capture_event_count();
  CHECK(!timer_capture_arm_compare(&driver, UINT16_C(12)));
  CHECK(events_match_from(
    offset,
    rejected_events,
    sizeof(rejected_events) / sizeof(rejected_events[0])
  ));
  CHECK(!driver.compare_armed);
  CHECK(mock_timer1_control() == TIMER1_CONTROL_READY);

  offset = mock_timer_capture_event_count();
  timer_capture_irq(&driver);
  CHECK(events_match_from(
    offset,
    overflow_events,
    sizeof(overflow_events) / sizeof(overflow_events[0])
  ));
  CHECK(driver.overflow_ticks == UINT32_C(0x10000));

  mock_timer_capture_latch_status(TIMER1_STATUS_COMPARE);
  offset = mock_timer_capture_event_count();
  timer_capture_irq(&driver);
  CHECK(events_match_from(
    offset,
    stale_compare_events,
    sizeof(stale_compare_events) / sizeof(stale_compare_events[0])
  ));
  CHECK(!driver.compare_pending);
  CHECK(mock_timer1_control() == TIMER1_CONTROL_READY);
  CHECK(!timer_capture_take_compare(&driver, &timestamp));
  CHECK(timestamp == 0u);
  CHECK(!mock_timer_capture_invalid_access());
  return true;
}

static bool test_capture_overrun_and_foreground_consumption(void) {
  timer_capture_t driver = { 0 };
  uint32_t timestamp = UINT32_MAX;
  const expected_event_t take_capture_events[] = {
    { MOCK_TIMER_CAPTURE_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0x5a) },
    { MOCK_TIMER_CAPTURE_EVENT_IRQ_RESTORE, UINT32_C(0x5a) },
  };
  size_t offset;

  mock_timer_capture_reset();
  CHECK(initialize(&driver));
  mock_timer_capture_latch_capture(UINT16_C(9));
  timer_capture_irq(&driver);
  mock_timer_capture_latch_capture(UINT16_C(12));
  timer_capture_irq(&driver);
  CHECK(driver.capture_pending);
  CHECK(driver.capture_timestamp == UINT32_C(9));
  CHECK(driver.capture_overruns == UINT32_C(1));

  mock_timer_capture_set_irq_state(UINT32_C(0x5a));
  offset = mock_timer_capture_event_count();
  CHECK(timer_capture_take_capture(&driver, &timestamp));
  CHECK(events_match_from(
    offset,
    take_capture_events,
    sizeof(take_capture_events) / sizeof(take_capture_events[0])
  ));
  CHECK(timestamp == UINT32_C(9));
  CHECK(timer_capture_take_overruns(&driver) == UINT32_C(1));
  CHECK(timer_capture_take_overruns(&driver) == 0u);
  CHECK(!timer_capture_take_capture(&driver, &timestamp));
  CHECK(timestamp == 0u);
  CHECK(!mock_timer_capture_invalid_access());
  return true;
}

static bool test_invalid_calls_and_idle_irq_boundaries(void) {
  timer_capture_t driver = { 0 };
  const timer_capture_t before = driver;
  uint32_t timestamp = UINT32_MAX;
  const expected_event_t idle_irq_events[] = {
    { MOCK_TIMER_CAPTURE_EVENT_STATUS_READ, 0u },
  };
  size_t offset;

  mock_timer_capture_reset();
  CHECK(!timer_capture_arm_compare(NULL, UINT16_C(1)));
  CHECK(!timer_capture_arm_compare(&driver, UINT16_C(1)));
  CHECK(!timer_capture_take_capture(NULL, &timestamp));
  CHECK(!timer_capture_take_capture(&driver, &timestamp));
  CHECK(!timer_capture_take_compare(NULL, &timestamp));
  CHECK(!timer_capture_take_compare(&driver, &timestamp));
  CHECK(timer_capture_take_overruns(NULL) == 0u);
  CHECK(timer_capture_take_overruns(&driver) == 0u);
  timer_capture_irq(NULL);
  timer_capture_irq(&driver);
  CHECK(state_equals(&driver, &before));
  CHECK(mock_timer_capture_event_count() == 0u);

  CHECK(initialize(&driver));
  offset = mock_timer_capture_event_count();
  CHECK(!timer_capture_arm_compare(&driver, 0u));
  CHECK(!timer_capture_arm_compare(
    &driver,
    (uint16_t)(TIMER_CAPTURE_MAX_DELAY + UINT16_C(1))
  ));
  CHECK(!timer_capture_take_capture(&driver, NULL));
  CHECK(!timer_capture_take_compare(&driver, NULL));
  CHECK(mock_timer_capture_event_count() == offset);

  timer_capture_irq(&driver);
  CHECK(events_match_from(
    offset,
    idle_irq_events,
    sizeof(idle_irq_events) / sizeof(idle_irq_events[0])
  ));
  CHECK(!mock_timer_capture_invalid_access());
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "initialization baseline", test_initialization_validation_and_baseline },
    { "capture overflow handoff", test_capture_timestamps_recover_across_overflow_handoff },
    { "compare wrap handoff", test_compare_arms_and_fires_once_across_counter_wrap },
    { "pending and stale status", test_pending_status_blocks_compare_and_stale_compare_is_ignored },
    { "capture overrun", test_capture_overrun_and_foreground_consumption },
    { "invalid and idle boundaries", test_invalid_calls_and_idle_irq_boundaries },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) {
      fprintf(stderr, "failed: %s\n", tests[index].name);
      return 1;
    }
  }

  printf("Timer capture/compare public tests passed (%zu tests).\n",
    sizeof(tests) / sizeof(tests[0]));
  return 0;
}

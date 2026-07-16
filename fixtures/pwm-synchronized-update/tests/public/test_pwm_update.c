#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "mock_pwm_update.h"
#include "pwm_update.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
              __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

typedef struct {
  mock_pwm_event_t event;
  uint32_t value;
} expected_event_t;

static bool initialize(
  pwm_update_t *driver,
  uint16_t period_ticks,
  uint16_t duty_ticks
) {
  return pwm_update_init(driver, mock_pwm0(), period_ticks, duty_ticks);
}

static bool state_equals(
  const pwm_update_t *left,
  const pwm_update_t *right
) {
  return left->pwm == right->pwm &&
    left->period_ticks == right->period_ticks &&
    left->active_duty_ticks == right->active_duty_ticks &&
    left->requested_duty_ticks == right->requested_duty_ticks &&
    left->event == right->event &&
    left->update_pending == right->update_pending &&
    left->faulted == right->faulted &&
    left->initialized == right->initialized;
}

static bool events_match_from(
  size_t offset,
  const expected_event_t *expected,
  size_t expected_count
) {
  if (mock_pwm_event_count() != offset + expected_count) return false;
  for (size_t index = 0u; index < expected_count; index++) {
    if (
      mock_pwm_event_at(offset + index) != expected[index].event ||
      mock_pwm_event_value(offset + index) != expected[index].value
    ) {
      return false;
    }
  }
  return true;
}

static bool test_initialization_validation_order_and_reinitialization(void) {
  pwm_update_t driver = {
    .pwm = (volatile pwm0_registers_t *)(uintptr_t)UINT32_C(1),
    .period_ticks = UINT16_C(2),
    .active_duty_ticks = UINT16_C(3),
    .requested_duty_ticks = UINT16_C(4),
    .event = PWM_UPDATE_EVENT_FAULT,
    .update_pending = true,
    .faulted = true,
    .initialized = true,
  };
  const pwm_update_t before = driver;
  const expected_event_t initialization_events[] = {
    { MOCK_PWM_EVENT_CONTROL_WRITE, 0u },
    { MOCK_PWM_EVENT_PERIOD_SHADOW_WRITE, UINT16_C(1000) },
    { MOCK_PWM_EVENT_COMPARE_SHADOW_WRITE, UINT16_C(250) },
    { MOCK_PWM_EVENT_STATUS_CLEAR_WRITE, PWM0_STATUS_ALL },
    { MOCK_PWM_EVENT_LOAD_WRITE, PWM0_LOAD_ALL },
    { MOCK_PWM_EVENT_CONTROL_WRITE, PWM0_CONTROL_READY },
  };
  size_t offset;

  mock_pwm_reset();
  mock_pwm_set_status(PWM0_STATUS_ALL);
  CHECK(!pwm_update_init(NULL, mock_pwm0(), UINT16_C(1000), UINT16_C(250)));
  CHECK(!pwm_update_init(&driver, NULL, UINT16_C(1000), UINT16_C(250)));
  CHECK(!pwm_update_init(&driver, mock_pwm0(), 0u, 0u));
  CHECK(!pwm_update_init(
    &driver,
    mock_pwm0(),
    UINT16_C(1000),
    UINT16_C(1001)
  ));
  CHECK(state_equals(&driver, &before));
  CHECK(mock_pwm_event_count() == 0u);

  CHECK(initialize(&driver, UINT16_C(1000), UINT16_C(250)));
  CHECK(events_match_from(
    0u,
    initialization_events,
    sizeof(initialization_events) / sizeof(initialization_events[0])
  ));
  CHECK(driver.pwm == mock_pwm0());
  CHECK(driver.period_ticks == UINT16_C(1000));
  CHECK(driver.active_duty_ticks == UINT16_C(250));
  CHECK(driver.requested_duty_ticks == UINT16_C(250));
  CHECK(driver.event == PWM_UPDATE_EVENT_NONE);
  CHECK(!driver.update_pending);
  CHECK(!driver.faulted);
  CHECK(driver.initialized);
  CHECK(mock_pwm_control() == PWM0_CONTROL_READY);
  CHECK(mock_pwm_period_shadow() == UINT16_C(1000));
  CHECK(mock_pwm_compare_shadow() == UINT16_C(250));
  CHECK(mock_pwm_active_period() == UINT16_C(1000));
  CHECK(mock_pwm_active_duty() == UINT16_C(250));
  CHECK(mock_pwm_load_pending() == 0u);
  CHECK(mock_pwm_status() == 0u);

  driver.active_duty_ticks = UINT16_C(999);
  driver.requested_duty_ticks = UINT16_C(999);
  driver.event = PWM_UPDATE_EVENT_FAULT;
  driver.update_pending = true;
  driver.faulted = true;
  offset = mock_pwm_event_count();
  CHECK(initialize(&driver, UINT16_C(1200), UINT16_C(600)));
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_PWM_EVENT_CONTROL_WRITE, 0u },
      { MOCK_PWM_EVENT_PERIOD_SHADOW_WRITE, UINT16_C(1200) },
      { MOCK_PWM_EVENT_COMPARE_SHADOW_WRITE, UINT16_C(600) },
      { MOCK_PWM_EVENT_STATUS_CLEAR_WRITE, PWM0_STATUS_ALL },
      { MOCK_PWM_EVENT_LOAD_WRITE, PWM0_LOAD_ALL },
      { MOCK_PWM_EVENT_CONTROL_WRITE, PWM0_CONTROL_READY },
    },
    6u
  ));
  CHECK(driver.period_ticks == UINT16_C(1200));
  CHECK(driver.active_duty_ticks == UINT16_C(600));
  CHECK(driver.requested_duty_ticks == UINT16_C(600));
  CHECK(driver.event == PWM_UPDATE_EVENT_NONE);
  CHECK(!driver.update_pending);
  CHECK(!driver.faulted);
  CHECK(driver.initialized);
  CHECK(!mock_pwm_invalid_access());
  return true;
}

static bool test_request_is_atomic_and_rejects_invalid_or_busy_state(void) {
  pwm_update_t driver = { 0 };
  pwm_update_t uninitialized = { 0 };
  pwm_update_t before;
  size_t offset;

  mock_pwm_reset();
  CHECK(!pwm_update_request_duty(NULL, UINT16_C(1)));
  CHECK(!pwm_update_request_duty(&uninitialized, UINT16_C(1)));
  CHECK(mock_pwm_event_count() == 0u);
  CHECK(initialize(&driver, UINT16_C(1000), UINT16_C(250)));

  offset = mock_pwm_event_count();
  before = driver;
  CHECK(!pwm_update_request_duty(&driver, UINT16_C(1001)));
  CHECK(state_equals(&driver, &before));
  CHECK(mock_pwm_event_count() == offset);

  mock_pwm_set_irq_state(UINT32_C(0xA5));
  offset = mock_pwm_event_count();
  CHECK(pwm_update_request_duty(&driver, UINT16_C(400)));
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_PWM_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xA5) },
      { MOCK_PWM_EVENT_STATUS_CLEAR_WRITE, PWM0_STATUS_UPDATE },
      { MOCK_PWM_EVENT_COMPARE_SHADOW_WRITE, UINT16_C(400) },
      { MOCK_PWM_EVENT_LOAD_WRITE, PWM0_LOAD_COMPARE },
      { MOCK_PWM_EVENT_IRQ_RESTORE, UINT32_C(0xA5) },
    },
    5u
  ));
  CHECK(driver.active_duty_ticks == UINT16_C(250));
  CHECK(driver.requested_duty_ticks == UINT16_C(400));
  CHECK(driver.update_pending);
  CHECK(driver.event == PWM_UPDATE_EVENT_NONE);
  CHECK(!driver.faulted);
  CHECK(mock_pwm_compare_shadow() == UINT16_C(400));
  CHECK(mock_pwm_active_duty() == UINT16_C(250));
  CHECK(mock_pwm_load_pending() == PWM0_LOAD_COMPARE);
  CHECK(mock_pwm_irq_state() == UINT32_C(0xA5));

  before = driver;
  offset = mock_pwm_event_count();
  CHECK(!pwm_update_request_duty(&driver, UINT16_C(300)));
  CHECK(state_equals(&driver, &before));
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_PWM_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xA5) },
      { MOCK_PWM_EVENT_IRQ_RESTORE, UINT32_C(0xA5) },
    },
    2u
  ));
  CHECK(!mock_pwm_invalid_access());
  return true;
}

static bool test_boundary_application_stale_status_and_event_consumption(void) {
  pwm_update_t driver = { 0 };
  pwm_update_t before;
  size_t offset;

  mock_pwm_reset();
  CHECK(initialize(&driver, UINT16_C(1000), UINT16_C(250)));
  CHECK(pwm_update_request_duty(&driver, UINT16_C(400)));
  CHECK(mock_pwm_active_duty() == UINT16_C(250));
  mock_pwm_trigger_period_boundary();
  CHECK(mock_pwm_active_duty() == UINT16_C(400));
  CHECK(mock_pwm_status() == PWM0_STATUS_UPDATE);
  CHECK(driver.active_duty_ticks == UINT16_C(250));

  offset = mock_pwm_event_count();
  pwm_update_irq(&driver);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_PWM_EVENT_STATUS_READ, PWM0_STATUS_UPDATE },
      { MOCK_PWM_EVENT_STATUS_CLEAR_WRITE, PWM0_STATUS_UPDATE },
    },
    2u
  ));
  CHECK(driver.active_duty_ticks == UINT16_C(400));
  CHECK(driver.requested_duty_ticks == UINT16_C(400));
  CHECK(!driver.update_pending);
  CHECK(driver.event == PWM_UPDATE_EVENT_APPLIED);
  CHECK(mock_pwm_status() == 0u);

  mock_pwm_set_irq_state(UINT32_C(0xC3));
  before = driver;
  offset = mock_pwm_event_count();
  CHECK(!pwm_update_request_duty(&driver, UINT16_C(500)));
  CHECK(state_equals(&driver, &before));
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_PWM_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xC3) },
      { MOCK_PWM_EVENT_IRQ_RESTORE, UINT32_C(0xC3) },
    },
    2u
  ));

  offset = mock_pwm_event_count();
  CHECK(pwm_update_take_event(&driver) == PWM_UPDATE_EVENT_APPLIED);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_PWM_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xC3) },
      { MOCK_PWM_EVENT_IRQ_RESTORE, UINT32_C(0xC3) },
    },
    2u
  ));
  CHECK(driver.event == PWM_UPDATE_EVENT_NONE);
  offset = mock_pwm_event_count();
  CHECK(pwm_update_take_event(&driver) == PWM_UPDATE_EVENT_NONE);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_PWM_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xC3) },
      { MOCK_PWM_EVENT_IRQ_RESTORE, UINT32_C(0xC3) },
    },
    2u
  ));

  mock_pwm_set_status(PWM0_STATUS_UPDATE);
  before = driver;
  offset = mock_pwm_event_count();
  pwm_update_irq(&driver);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_PWM_EVENT_STATUS_READ, PWM0_STATUS_UPDATE },
      { MOCK_PWM_EVENT_STATUS_CLEAR_WRITE, PWM0_STATUS_UPDATE },
    },
    2u
  ));
  CHECK(state_equals(&driver, &before));
  CHECK(mock_pwm_status() == 0u);

  CHECK(pwm_update_request_duty(&driver, UINT16_C(500)));
  CHECK(driver.update_pending);
  CHECK(!mock_pwm_invalid_access());
  return true;
}

static bool test_fault_priority_safe_recovery_and_event_gating(void) {
  pwm_update_t driver = { 0 };
  pwm_update_t before;
  size_t offset;

  mock_pwm_reset();
  CHECK(initialize(&driver, UINT16_C(1000), UINT16_C(250)));
  CHECK(pwm_update_request_duty(&driver, UINT16_C(400)));
  mock_pwm_trigger_period_boundary();
  mock_pwm_raise_fault();
  CHECK(mock_pwm_status() == PWM0_STATUS_ALL);
  CHECK(mock_pwm_active_duty() == UINT16_C(400));

  offset = mock_pwm_event_count();
  pwm_update_irq(&driver);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_PWM_EVENT_STATUS_READ, PWM0_STATUS_ALL },
      { MOCK_PWM_EVENT_CONTROL_WRITE, 0u },
      { MOCK_PWM_EVENT_STATUS_CLEAR_WRITE, PWM0_STATUS_ALL },
    },
    3u
  ));
  CHECK(driver.active_duty_ticks == UINT16_C(250));
  CHECK(driver.requested_duty_ticks == UINT16_C(400));
  CHECK(!driver.update_pending);
  CHECK(driver.faulted);
  CHECK(driver.event == PWM_UPDATE_EVENT_FAULT);
  CHECK(mock_pwm_control() == 0u);
  CHECK(mock_pwm_status() == 0u);

  mock_pwm_set_irq_state(UINT32_C(0x3C));
  before = driver;
  offset = mock_pwm_event_count();
  CHECK(!pwm_update_request_duty(&driver, UINT16_C(300)));
  CHECK(state_equals(&driver, &before));
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_PWM_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0x3C) },
      { MOCK_PWM_EVENT_IRQ_RESTORE, UINT32_C(0x3C) },
    },
    2u
  ));

  offset = mock_pwm_event_count();
  CHECK(pwm_update_recover(&driver));
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_PWM_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0x3C) },
      { MOCK_PWM_EVENT_CONTROL_WRITE, 0u },
      { MOCK_PWM_EVENT_PERIOD_SHADOW_WRITE, UINT16_C(1000) },
      { MOCK_PWM_EVENT_COMPARE_SHADOW_WRITE, UINT16_C(250) },
      { MOCK_PWM_EVENT_STATUS_CLEAR_WRITE, PWM0_STATUS_ALL },
      { MOCK_PWM_EVENT_LOAD_WRITE, PWM0_LOAD_ALL },
      { MOCK_PWM_EVENT_CONTROL_WRITE, PWM0_CONTROL_READY },
      { MOCK_PWM_EVENT_IRQ_RESTORE, UINT32_C(0x3C) },
    },
    8u
  ));
  CHECK(driver.active_duty_ticks == UINT16_C(250));
  CHECK(driver.requested_duty_ticks == UINT16_C(250));
  CHECK(!driver.update_pending);
  CHECK(!driver.faulted);
  CHECK(driver.event == PWM_UPDATE_EVENT_FAULT);
  CHECK(mock_pwm_active_period() == UINT16_C(1000));
  CHECK(mock_pwm_active_duty() == UINT16_C(250));
  CHECK(mock_pwm_control() == PWM0_CONTROL_READY);

  before = driver;
  offset = mock_pwm_event_count();
  CHECK(!pwm_update_recover(&driver));
  CHECK(state_equals(&driver, &before));
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_PWM_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0x3C) },
      { MOCK_PWM_EVENT_IRQ_RESTORE, UINT32_C(0x3C) },
    },
    2u
  ));

  offset = mock_pwm_event_count();
  CHECK(!pwm_update_request_duty(&driver, UINT16_C(300)));
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_PWM_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0x3C) },
      { MOCK_PWM_EVENT_IRQ_RESTORE, UINT32_C(0x3C) },
    },
    2u
  ));
  CHECK(pwm_update_take_event(&driver) == PWM_UPDATE_EVENT_FAULT);
  CHECK(pwm_update_request_duty(&driver, UINT16_C(300)));
  CHECK(!mock_pwm_invalid_access());
  return true;
}

static bool test_fault_without_request_and_invalid_side_effects(void) {
  pwm_update_t driver = { 0 };
  pwm_update_t uninitialized = { 0 };
  size_t offset;

  mock_pwm_reset();
  pwm_update_irq(NULL);
  pwm_update_irq(&uninitialized);
  CHECK(!pwm_update_recover(NULL));
  CHECK(!pwm_update_recover(&uninitialized));
  CHECK(pwm_update_take_event(NULL) == PWM_UPDATE_EVENT_NONE);
  CHECK(pwm_update_take_event(&uninitialized) == PWM_UPDATE_EVENT_NONE);
  CHECK(mock_pwm_event_count() == 0u);

  CHECK(initialize(&driver, UINT16_C(1000), UINT16_C(250)));
  mock_pwm_raise_fault();
  offset = mock_pwm_event_count();
  pwm_update_irq(&driver);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_PWM_EVENT_STATUS_READ, PWM0_STATUS_FAULT },
      { MOCK_PWM_EVENT_CONTROL_WRITE, 0u },
      { MOCK_PWM_EVENT_STATUS_CLEAR_WRITE, PWM0_STATUS_ALL },
    },
    3u
  ));
  CHECK(driver.faulted);
  CHECK(!driver.update_pending);
  CHECK(driver.event == PWM_UPDATE_EVENT_FAULT);
  CHECK(mock_pwm_control() == 0u);

  mock_pwm_set_status(PWM0_STATUS_FAULT);
  offset = mock_pwm_event_count();
  pwm_update_irq(&driver);
  CHECK(mock_pwm_event_count() == offset);
  CHECK(mock_pwm_status() == PWM0_STATUS_FAULT);
  CHECK(!mock_pwm_invalid_access());
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "initialization", test_initialization_validation_order_and_reinitialization },
    { "request", test_request_is_atomic_and_rejects_invalid_or_busy_state },
    { "boundary", test_boundary_application_stale_status_and_event_consumption },
    { "fault-recovery", test_fault_priority_safe_recovery_and_event_gating },
    { "fault-invalid", test_fault_without_request_and_invalid_side_effects },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) {
      fprintf(stderr, "PWM public test failed: %s\n", tests[index].name);
      return 1;
    }
  }
  printf(
    "PWM synchronized-update public tests passed (%zu tests).\n",
    sizeof(tests) / sizeof(tests[0])
  );
  return 0;
}

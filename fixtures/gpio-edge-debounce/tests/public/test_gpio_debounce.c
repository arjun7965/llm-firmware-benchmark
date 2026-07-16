#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "gpio_debounce.h"
#include "mock_gpio_debounce.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
              __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

typedef struct {
  mock_gpio_event_t event;
  uint32_t value;
} expected_event_t;

static bool initialize(gpio_debounce_t *driver) {
  return gpio_debounce_init(driver, mock_gpio0());
}

static bool state_equals(
  const gpio_debounce_t *left,
  const gpio_debounce_t *right
) {
  return left->gpio == right->gpio &&
    left->stable_pressed == right->stable_pressed &&
    left->candidate_pressed == right->candidate_pressed &&
    left->debounce_active == right->debounce_active &&
    left->sleeping == right->sleeping &&
    left->candidate_started_at_ms == right->candidate_started_at_ms &&
    left->initialized == right->initialized;
}

static bool events_match_from(
  size_t offset,
  const expected_event_t *expected,
  size_t expected_count
) {
  if (mock_gpio_event_count() != offset + expected_count) return false;
  for (size_t index = 0u; index < expected_count; index++) {
    if (mock_gpio_event_at(offset + index) != expected[index].event ||
        mock_gpio_event_value(offset + index) != expected[index].value) {
      return false;
    }
  }
  return true;
}

static bool begin_press(gpio_debounce_t *driver, uint32_t now_ms) {
  mock_gpio_set_button_pressed(true);
  gpio_debounce_irq(driver, now_ms);
  return driver->debounce_active && driver->candidate_pressed &&
    driver->candidate_started_at_ms == now_ms;
}

static bool test_initialization_validation_order_and_reinitialization(void) {
  gpio_debounce_t driver = {
    .gpio = (volatile gpio0_registers_t *)(uintptr_t)UINT32_C(1),
    .stable_pressed = true,
    .candidate_pressed = false,
    .debounce_active = true,
    .sleeping = true,
    .candidate_started_at_ms = 99u,
    .initialized = true,
  };
  const gpio_debounce_t before = driver;
  const expected_event_t initialization_events[] = {
    { MOCK_GPIO_EVENT_IRQ_ENABLE_WRITE, 0u },
    { MOCK_GPIO_EVENT_WAKE_ENABLE_WRITE, 0u },
    { MOCK_GPIO_EVENT_RISING_ENABLE_WRITE, GPIO0_BUTTON_MASK },
    { MOCK_GPIO_EVENT_FALLING_ENABLE_WRITE, GPIO0_BUTTON_MASK },
    { MOCK_GPIO_EVENT_EDGE_CLEAR_WRITE, GPIO0_BUTTON_MASK },
    { MOCK_GPIO_EVENT_WAKE_CLEAR_WRITE, GPIO0_BUTTON_MASK },
    { MOCK_GPIO_EVENT_INPUT_READ, 0u },
    { MOCK_GPIO_EVENT_IRQ_ENABLE_WRITE, GPIO0_BUTTON_MASK },
  };
  size_t offset;

  mock_gpio_reset();
  mock_gpio_set_button_pressed(true);
  mock_gpio_set_edge_status(GPIO0_BUTTON_MASK);
  mock_gpio_set_wake_status(GPIO0_BUTTON_MASK);
  CHECK(!gpio_debounce_init(NULL, mock_gpio0()));
  CHECK(!gpio_debounce_init(&driver, NULL));
  CHECK(state_equals(&driver, &before));
  CHECK(mock_gpio_event_count() == 0u);

  CHECK(initialize(&driver));
  CHECK(events_match_from(
    0u,
    initialization_events,
    sizeof(initialization_events) / sizeof(initialization_events[0])
  ));
  CHECK(driver.gpio == mock_gpio0());
  CHECK(driver.stable_pressed);
  CHECK(driver.candidate_pressed);
  CHECK(!driver.debounce_active);
  CHECK(!driver.sleeping);
  CHECK(driver.candidate_started_at_ms == 0u);
  CHECK(driver.initialized);
  CHECK(mock_gpio_rising_enable() == GPIO0_BUTTON_MASK);
  CHECK(mock_gpio_falling_enable() == GPIO0_BUTTON_MASK);
  CHECK(mock_gpio_irq_enable() == GPIO0_BUTTON_MASK);
  CHECK(mock_gpio_wake_enable() == 0u);
  CHECK(mock_gpio_edge_status() == 0u);
  CHECK(mock_gpio_wake_status() == 0u);

  mock_gpio_set_button_pressed(false);
  offset = mock_gpio_event_count();
  CHECK(initialize(&driver));
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_GPIO_EVENT_IRQ_ENABLE_WRITE, 0u },
      { MOCK_GPIO_EVENT_WAKE_ENABLE_WRITE, 0u },
      { MOCK_GPIO_EVENT_RISING_ENABLE_WRITE, GPIO0_BUTTON_MASK },
      { MOCK_GPIO_EVENT_FALLING_ENABLE_WRITE, GPIO0_BUTTON_MASK },
      { MOCK_GPIO_EVENT_EDGE_CLEAR_WRITE, GPIO0_BUTTON_MASK },
      { MOCK_GPIO_EVENT_WAKE_CLEAR_WRITE, GPIO0_BUTTON_MASK },
      { MOCK_GPIO_EVENT_INPUT_READ, GPIO0_BUTTON_MASK },
      { MOCK_GPIO_EVENT_IRQ_ENABLE_WRITE, GPIO0_BUTTON_MASK },
    },
    8u
  ));
  CHECK(!driver.stable_pressed);
  CHECK(!driver.candidate_pressed);
  CHECK(!driver.debounce_active);
  CHECK(!driver.sleeping);
  CHECK(!mock_gpio_invalid_access());
  return true;
}

static bool test_irq_captures_one_edge_and_disables_delivery(void) {
  gpio_debounce_t driver = { 0 };
  gpio_debounce_t uninitialized = { 0 };
  gpio_debounce_t before;
  size_t offset;

  mock_gpio_reset();
  gpio_debounce_irq(NULL, 1u);
  gpio_debounce_irq(&uninitialized, 1u);
  CHECK(mock_gpio_event_count() == 0u);
  CHECK(initialize(&driver));

  mock_gpio_set_edge_status(UINT32_C(1) << 1);
  before = driver;
  offset = mock_gpio_event_count();
  gpio_debounce_irq(&driver, 10u);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_GPIO_EVENT_EDGE_STATUS_READ, UINT32_C(1) << 1 },
    },
    1u
  ));
  CHECK(state_equals(&driver, &before));

  mock_gpio_set_edge_status(0u);
  mock_gpio_set_button_pressed(true);
  offset = mock_gpio_event_count();
  gpio_debounce_irq(&driver, 100u);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_GPIO_EVENT_EDGE_STATUS_READ, GPIO0_BUTTON_MASK },
      { MOCK_GPIO_EVENT_IRQ_ENABLE_WRITE, 0u },
      { MOCK_GPIO_EVENT_EDGE_CLEAR_WRITE, GPIO0_BUTTON_MASK },
      { MOCK_GPIO_EVENT_INPUT_READ, 0u },
    },
    4u
  ));
  CHECK(driver.debounce_active);
  CHECK(driver.candidate_pressed);
  CHECK(driver.candidate_started_at_ms == 100u);
  CHECK(mock_gpio_irq_enable() == 0u);
  CHECK(mock_gpio_edge_status() == 0u);

  mock_gpio_set_edge_status(GPIO0_BUTTON_MASK);
  before = driver;
  offset = mock_gpio_event_count();
  gpio_debounce_irq(&driver, 101u);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_GPIO_EVENT_EDGE_STATUS_READ, GPIO0_BUTTON_MASK },
      { MOCK_GPIO_EVENT_IRQ_ENABLE_WRITE, 0u },
      { MOCK_GPIO_EVENT_EDGE_CLEAR_WRITE, GPIO0_BUTTON_MASK },
    },
    3u
  ));
  CHECK(state_equals(&driver, &before));
  CHECK(!mock_gpio_invalid_access());
  return true;
}

static bool test_debounce_deadline_release_and_interrupt_restoration(void) {
  gpio_debounce_t driver = { 0 };
  size_t offset;

  mock_gpio_reset();
  CHECK(initialize(&driver));
  CHECK(begin_press(&driver, 100u));
  CHECK(!gpio_debounce_is_pressed(&driver));
  mock_gpio_set_irq_state(UINT32_C(0xA5));

  offset = mock_gpio_event_count();
  CHECK(gpio_debounce_poll(&driver, 119u) == GPIO_DEBOUNCE_EVENT_NONE);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_GPIO_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xA5) },
      { MOCK_GPIO_EVENT_IRQ_RESTORE, UINT32_C(0xA5) },
    },
    2u
  ));
  CHECK(driver.debounce_active);
  CHECK(mock_gpio_irq_state() == UINT32_C(0xA5));

  offset = mock_gpio_event_count();
  CHECK(gpio_debounce_poll(&driver, 120u) == GPIO_DEBOUNCE_EVENT_PRESSED);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_GPIO_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xA5) },
      { MOCK_GPIO_EVENT_INPUT_READ, 0u },
      { MOCK_GPIO_EVENT_IRQ_ENABLE_WRITE, GPIO0_BUTTON_MASK },
      { MOCK_GPIO_EVENT_IRQ_RESTORE, UINT32_C(0xA5) },
    },
    4u
  ));
  CHECK(!driver.debounce_active);
  CHECK(driver.stable_pressed);
  CHECK(mock_gpio_irq_enable() == GPIO0_BUTTON_MASK);

  offset = mock_gpio_event_count();
  CHECK(gpio_debounce_is_pressed(&driver));
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_GPIO_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xA5) },
      { MOCK_GPIO_EVENT_IRQ_RESTORE, UINT32_C(0xA5) },
    },
    2u
  ));
  offset = mock_gpio_event_count();
  CHECK(gpio_debounce_poll(&driver, 121u) == GPIO_DEBOUNCE_EVENT_NONE);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_GPIO_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xA5) },
      { MOCK_GPIO_EVENT_IRQ_RESTORE, UINT32_C(0xA5) },
    },
    2u
  ));

  mock_gpio_set_button_pressed(false);
  gpio_debounce_irq(&driver, 130u);
  CHECK(driver.debounce_active);
  CHECK(!driver.candidate_pressed);
  CHECK(gpio_debounce_poll(&driver, 150u) == GPIO_DEBOUNCE_EVENT_RELEASED);
  CHECK(!driver.stable_pressed);
  CHECK(!gpio_debounce_is_pressed(&driver));
  CHECK(mock_gpio_irq_enable() == GPIO0_BUTTON_MASK);
  CHECK(!mock_gpio_invalid_access());
  return true;
}

static bool test_bounce_restarts_the_interval_and_retains_late_edges(void) {
  gpio_debounce_t driver = { 0 };

  mock_gpio_reset();
  CHECK(initialize(&driver));
  CHECK(begin_press(&driver, 100u));
  mock_gpio_set_button_pressed(false);
  CHECK(mock_gpio_edge_status() == GPIO0_BUTTON_MASK);

  CHECK(gpio_debounce_poll(&driver, 120u) == GPIO_DEBOUNCE_EVENT_NONE);
  CHECK(driver.debounce_active);
  CHECK(!driver.candidate_pressed);
  CHECK(driver.candidate_started_at_ms == 120u);
  CHECK(mock_gpio_irq_enable() == 0u);
  CHECK(mock_gpio_edge_status() == GPIO0_BUTTON_MASK);

  CHECK(gpio_debounce_poll(&driver, 139u) == GPIO_DEBOUNCE_EVENT_NONE);
  CHECK(driver.debounce_active);
  CHECK(mock_gpio_irq_enable() == 0u);
  CHECK(gpio_debounce_poll(&driver, 140u) == GPIO_DEBOUNCE_EVENT_NONE);
  CHECK(!driver.debounce_active);
  CHECK(!driver.stable_pressed);
  CHECK(mock_gpio_irq_enable() == GPIO0_BUTTON_MASK);
  CHECK(mock_gpio_edge_status() == GPIO0_BUTTON_MASK);

  gpio_debounce_irq(&driver, 141u);
  CHECK(driver.debounce_active);
  CHECK(!driver.candidate_pressed);
  CHECK(driver.candidate_started_at_ms == 141u);
  CHECK(mock_gpio_edge_status() == 0u);
  CHECK(gpio_debounce_poll(&driver, 161u) == GPIO_DEBOUNCE_EVENT_NONE);
  CHECK(mock_gpio_irq_enable() == GPIO0_BUTTON_MASK);
  CHECK(!mock_gpio_invalid_access());
  return true;
}

static bool test_wrap_safe_debounce_boundary(void) {
  gpio_debounce_t driver = { 0 };
  const uint32_t started_at = UINT32_MAX - UINT32_C(10);

  mock_gpio_reset();
  CHECK(initialize(&driver));
  CHECK(begin_press(&driver, started_at));
  CHECK(gpio_debounce_poll(&driver, UINT32_MAX - UINT32_C(5)) ==
    GPIO_DEBOUNCE_EVENT_NONE);
  CHECK(driver.debounce_active);
  CHECK(gpio_debounce_poll(&driver, UINT32_C(8)) ==
    GPIO_DEBOUNCE_EVENT_NONE);
  CHECK(driver.debounce_active);
  CHECK(gpio_debounce_poll(&driver, UINT32_C(9)) ==
    GPIO_DEBOUNCE_EVENT_PRESSED);
  CHECK(!driver.debounce_active);
  CHECK(driver.stable_pressed);
  CHECK(!mock_gpio_invalid_access());
  return true;
}

static bool test_sleep_wake_recovery_and_stale_latch_clearing(void) {
  gpio_debounce_t driver = { 0 };
  size_t offset;

  mock_gpio_reset();
  CHECK(initialize(&driver));
  mock_gpio_set_irq_state(UINT32_C(0x9C));
  offset = mock_gpio_event_count();
  CHECK(gpio_debounce_prepare_sleep(&driver));
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_GPIO_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0x9C) },
      { MOCK_GPIO_EVENT_IRQ_ENABLE_WRITE, 0u },
      { MOCK_GPIO_EVENT_EDGE_CLEAR_WRITE, GPIO0_BUTTON_MASK },
      { MOCK_GPIO_EVENT_WAKE_CLEAR_WRITE, GPIO0_BUTTON_MASK },
      { MOCK_GPIO_EVENT_WAKE_ENABLE_WRITE, GPIO0_BUTTON_MASK },
      { MOCK_GPIO_EVENT_IRQ_RESTORE, UINT32_C(0x9C) },
    },
    6u
  ));
  CHECK(driver.sleeping);
  CHECK(mock_gpio_irq_enable() == 0u);
  CHECK(mock_gpio_wake_enable() == GPIO0_BUTTON_MASK);

  mock_gpio_set_button_pressed(true);
  CHECK(mock_gpio_edge_status() == GPIO0_BUTTON_MASK);
  CHECK(mock_gpio_wake_status() == GPIO0_BUTTON_MASK);
  offset = mock_gpio_event_count();
  gpio_debounce_irq(&driver, 199u);
  CHECK(mock_gpio_event_count() == offset);

  offset = mock_gpio_event_count();
  CHECK(gpio_debounce_resume(&driver, 200u));
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_GPIO_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0x9C) },
      { MOCK_GPIO_EVENT_WAKE_ENABLE_WRITE, 0u },
      { MOCK_GPIO_EVENT_EDGE_CLEAR_WRITE, GPIO0_BUTTON_MASK },
      { MOCK_GPIO_EVENT_WAKE_CLEAR_WRITE, GPIO0_BUTTON_MASK },
      { MOCK_GPIO_EVENT_INPUT_READ, 0u },
      { MOCK_GPIO_EVENT_IRQ_RESTORE, UINT32_C(0x9C) },
    },
    6u
  ));
  CHECK(!driver.sleeping);
  CHECK(driver.debounce_active);
  CHECK(driver.candidate_pressed);
  CHECK(driver.candidate_started_at_ms == 200u);
  CHECK(mock_gpio_irq_enable() == 0u);
  CHECK(mock_gpio_wake_enable() == 0u);
  CHECK(mock_gpio_edge_status() == 0u);
  CHECK(mock_gpio_wake_status() == 0u);
  CHECK(gpio_debounce_poll(&driver, 219u) == GPIO_DEBOUNCE_EVENT_NONE);
  CHECK(gpio_debounce_poll(&driver, 220u) == GPIO_DEBOUNCE_EVENT_PRESSED);
  CHECK(mock_gpio_irq_enable() == GPIO0_BUTTON_MASK);
  CHECK(!mock_gpio_invalid_access());
  return true;
}

static bool test_resume_same_level_rearms_normal_interrupts(void) {
  gpio_debounce_t driver = { 0 };
  size_t offset;

  mock_gpio_reset();
  CHECK(initialize(&driver));
  CHECK(gpio_debounce_prepare_sleep(&driver));
  mock_gpio_set_edge_status(GPIO0_BUTTON_MASK);
  mock_gpio_set_wake_status(GPIO0_BUTTON_MASK);
  offset = mock_gpio_event_count();
  CHECK(gpio_debounce_resume(&driver, 77u));
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_GPIO_EVENT_IRQ_SAVE_DISABLE, UINT32_C(1) },
      { MOCK_GPIO_EVENT_WAKE_ENABLE_WRITE, 0u },
      { MOCK_GPIO_EVENT_EDGE_CLEAR_WRITE, GPIO0_BUTTON_MASK },
      { MOCK_GPIO_EVENT_WAKE_CLEAR_WRITE, GPIO0_BUTTON_MASK },
      { MOCK_GPIO_EVENT_INPUT_READ, GPIO0_BUTTON_MASK },
      { MOCK_GPIO_EVENT_IRQ_ENABLE_WRITE, GPIO0_BUTTON_MASK },
      { MOCK_GPIO_EVENT_IRQ_RESTORE, UINT32_C(1) },
    },
    7u
  ));
  CHECK(!driver.sleeping);
  CHECK(!driver.debounce_active);
  CHECK(!driver.stable_pressed);
  CHECK(mock_gpio_irq_enable() == GPIO0_BUTTON_MASK);
  CHECK(mock_gpio_wake_enable() == 0u);
  CHECK(mock_gpio_edge_status() == 0u);
  CHECK(mock_gpio_wake_status() == 0u);
  CHECK(gpio_debounce_poll(&driver, 100u) == GPIO_DEBOUNCE_EVENT_NONE);
  CHECK(!mock_gpio_invalid_access());
  return true;
}

static bool test_invalid_foreground_calls_and_sleep_rejection(void) {
  gpio_debounce_t driver = { 0 };
  gpio_debounce_t uninitialized = { 0 };
  size_t offset;

  mock_gpio_reset();
  CHECK(gpio_debounce_poll(NULL, 0u) == GPIO_DEBOUNCE_EVENT_NONE);
  CHECK(gpio_debounce_poll(&uninitialized, 0u) == GPIO_DEBOUNCE_EVENT_NONE);
  CHECK(!gpio_debounce_prepare_sleep(NULL));
  CHECK(!gpio_debounce_prepare_sleep(&uninitialized));
  CHECK(!gpio_debounce_resume(NULL, 0u));
  CHECK(!gpio_debounce_resume(&uninitialized, 0u));
  CHECK(!gpio_debounce_is_pressed(NULL));
  CHECK(!gpio_debounce_is_pressed(&uninitialized));
  CHECK(mock_gpio_event_count() == 0u);

  CHECK(initialize(&driver));
  offset = mock_gpio_event_count();
  CHECK(!gpio_debounce_resume(&driver, 1u));
  CHECK(mock_gpio_event_count() == offset);
  CHECK(begin_press(&driver, 10u));
  mock_gpio_set_irq_state(UINT32_C(0x44));
  offset = mock_gpio_event_count();
  CHECK(!gpio_debounce_prepare_sleep(&driver));
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_GPIO_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0x44) },
      { MOCK_GPIO_EVENT_IRQ_RESTORE, UINT32_C(0x44) },
    },
    2u
  ));
  CHECK(!driver.sleeping);
  CHECK(driver.debounce_active);
  CHECK(mock_gpio_irq_enable() == 0u);
  CHECK(mock_gpio_irq_state() == UINT32_C(0x44));
  CHECK(!mock_gpio_invalid_access());
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "initialization validation and ordering", test_initialization_validation_order_and_reinitialization },
    { "ISR edge capture", test_irq_captures_one_edge_and_disables_delivery },
    { "deadline, release, and interrupt restoration", test_debounce_deadline_release_and_interrupt_restoration },
    { "bounce restart and late-edge retention", test_bounce_restarts_the_interval_and_retains_late_edges },
    { "wrap-safe debounce boundary", test_wrap_safe_debounce_boundary },
    { "sleep wake recovery", test_sleep_wake_recovery_and_stale_latch_clearing },
    { "same-level wake resume", test_resume_same_level_rearms_normal_interrupts },
    { "invalid and sleep rejection", test_invalid_foreground_calls_and_sleep_rejection },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) {
      fprintf(stderr, "test failed: %s\n", tests[index].name);
      return 1;
    }
  }
  printf("GPIO edge/debounce public tests passed.\n");
  return 0;
}

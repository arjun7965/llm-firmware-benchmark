#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "low_power_wake_clock.h"
#include "mock_low_power_wake_clock.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
        __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

typedef struct {
  mock_pwrclk_event_t event;
  uint32_t value;
} expected_event_t;

static bool state_equals(
  const power_manager_t *left,
  const power_manager_t *right
) {
  return left->registers == right->registers &&
    left->wake_mask == right->wake_mask &&
    left->sleep_mode == right->sleep_mode &&
    left->sleeping == right->sleeping &&
    left->initialized == right->initialized;
}

static bool events_match_from(
  size_t offset,
  const expected_event_t *expected,
  size_t expected_count
) {
  if (mock_pwrclk_event_count() != offset + expected_count) return false;
  for (size_t index = 0u; index < expected_count; index++) {
    if (mock_pwrclk_event_at(offset + index) != expected[index].event ||
        mock_pwrclk_event_value(offset + index) != expected[index].value) {
      return false;
    }
  }
  return true;
}

static bool initialize(power_manager_t *manager) {
  return power_manager_init(manager, mock_pwrclk0());
}

static bool test_initialization_validation_order_and_clock(void) {
  power_manager_t manager = {
    .registers = (volatile pwrclk0_registers_t *)(uintptr_t)UINT32_C(1),
    .wake_mask = PWRCLK0_WAKE_ALL,
    .sleep_mode = POWER_SLEEP_MODE_DEEP,
    .sleeping = true,
    .initialized = true,
  };
  const power_manager_t before = manager;
  const expected_event_t expected[] = {
    { MOCK_PWRCLK_EVENT_WAKE_ENABLE_WRITE, 0u },
    { MOCK_PWRCLK_EVENT_WAKE_CLEAR_WRITE, PWRCLK0_WAKE_ALL },
    { MOCK_PWRCLK_EVENT_CLOCK_WRITE, PWRCLK0_CLOCK_RUN },
    { MOCK_PWRCLK_EVENT_SLEEP_MODE_WRITE, PWRCLK0_SLEEP_NONE },
  };

  mock_pwrclk_reset();
  mock_pwrclk_set_wake_status(PWRCLK0_WAKE_ALL);
  CHECK(!power_manager_init(NULL, mock_pwrclk0()));
  CHECK(!power_manager_init(&manager, NULL));
  CHECK(state_equals(&manager, &before));
  CHECK(mock_pwrclk_event_count() == 0u);

  CHECK(initialize(&manager));
  CHECK(events_match_from(0u, expected, sizeof(expected) / sizeof(expected[0])));
  CHECK(manager.registers == mock_pwrclk0());
  CHECK(manager.wake_mask == 0u);
  CHECK(manager.sleep_mode == POWER_SLEEP_MODE_IDLE);
  CHECK(!manager.sleeping);
  CHECK(manager.initialized);
  CHECK(mock_pwrclk_wake_enable() == 0u);
  CHECK(mock_pwrclk_wake_status() == 0u);
  CHECK(mock_pwrclk_clock() == PWRCLK0_CLOCK_RUN);
  CHECK(mock_pwrclk_sleep_mode() == PWRCLK0_SLEEP_NONE);
  CHECK(power_manager_clock_hz(&manager) == POWER_RUN_CLOCK_HZ);
  CHECK(power_manager_clock_hz(NULL) == 0u);
  CHECK(!mock_pwrclk_invalid_access());
  return true;
}

static bool test_idle_validation_and_ordered_wake_arming(void) {
  power_manager_t manager = { 0 };
  power_manager_t before;
  size_t offset;

  mock_pwrclk_reset();
  CHECK(initialize(&manager));
  before = manager;
  offset = mock_pwrclk_event_count();
  CHECK(!power_manager_prepare_sleep(
    &manager,
    POWER_SLEEP_MODE_COUNT,
    PWRCLK0_WAKE_GPIO
  ));
  CHECK(!power_manager_prepare_sleep(&manager, POWER_SLEEP_MODE_IDLE, 0u));
  CHECK(!power_manager_prepare_sleep(
    &manager,
    POWER_SLEEP_MODE_DEEP,
    PWRCLK0_WAKE_UART
  ));
  CHECK(!power_manager_prepare_sleep(
    &manager,
    POWER_SLEEP_MODE_IDLE,
    PWRCLK0_WAKE_ALL | (UINT32_C(1) << 8)
  ));
  CHECK(state_equals(&manager, &before));
  CHECK(mock_pwrclk_event_count() == offset);

  mock_pwrclk_set_irq_state(UINT32_C(0xA5));
  offset = mock_pwrclk_event_count();
  CHECK(power_manager_prepare_sleep(
    &manager,
    POWER_SLEEP_MODE_IDLE,
    PWRCLK0_WAKE_GPIO | PWRCLK0_WAKE_UART
  ));
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_PWRCLK_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xA5) },
      { MOCK_PWRCLK_EVENT_WAKE_ENABLE_WRITE, 0u },
      { MOCK_PWRCLK_EVENT_WAKE_CLEAR_WRITE, PWRCLK0_WAKE_ALL },
      { MOCK_PWRCLK_EVENT_CLOCK_WRITE, PWRCLK0_CLOCK_RUN },
      { MOCK_PWRCLK_EVENT_WAKE_ENABLE_WRITE,
        PWRCLK0_WAKE_GPIO | PWRCLK0_WAKE_UART },
      { MOCK_PWRCLK_EVENT_SLEEP_MODE_WRITE, PWRCLK0_SLEEP_IDLE },
      { MOCK_PWRCLK_EVENT_IRQ_RESTORE, UINT32_C(0xA5) },
    },
    7u
  ));
  CHECK(manager.sleeping);
  CHECK(manager.sleep_mode == POWER_SLEEP_MODE_IDLE);
  CHECK(manager.wake_mask == (PWRCLK0_WAKE_GPIO | PWRCLK0_WAKE_UART));
  CHECK(mock_pwrclk_clock() == PWRCLK0_CLOCK_RUN);
  CHECK(mock_pwrclk_wake_enable() ==
    (PWRCLK0_WAKE_GPIO | PWRCLK0_WAKE_UART));
  CHECK(mock_pwrclk_sleep_mode() == PWRCLK0_SLEEP_IDLE);
  CHECK(mock_pwrclk_irq_state() == UINT32_C(0xA5));

  offset = mock_pwrclk_event_count();
  CHECK(!power_manager_prepare_sleep(
    &manager,
    POWER_SLEEP_MODE_IDLE,
    PWRCLK0_WAKE_GPIO
  ));
  CHECK(mock_pwrclk_event_count() == offset);
  CHECK(!mock_pwrclk_invalid_access());
  return true;
}

static bool test_deep_clock_transition_no_wake_and_priority_resume(void) {
  power_manager_t manager = { 0 };
  size_t offset;

  mock_pwrclk_reset();
  CHECK(initialize(&manager));
  mock_pwrclk_set_wake_status(PWRCLK0_WAKE_UART);
  mock_pwrclk_set_irq_state(UINT32_C(0x9C));
  CHECK(power_manager_prepare_sleep(
    &manager,
    POWER_SLEEP_MODE_DEEP,
    PWRCLK0_WAKE_RTC | PWRCLK0_WAKE_GPIO
  ));
  CHECK(mock_pwrclk_wake_status() == 0u);
  CHECK(mock_pwrclk_clock() == PWRCLK0_CLOCK_SLEEP);
  CHECK(mock_pwrclk_sleep_mode() == PWRCLK0_SLEEP_DEEP);
  CHECK(power_manager_clock_hz(&manager) == POWER_SLEEP_CLOCK_HZ);

  mock_pwrclk_set_wake_status(PWRCLK0_WAKE_UART);
  offset = mock_pwrclk_event_count();
  CHECK(power_manager_resume(&manager) == POWER_WAKE_NONE);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_PWRCLK_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0x9C) },
      { MOCK_PWRCLK_EVENT_WAKE_STATUS_READ, PWRCLK0_WAKE_UART },
      { MOCK_PWRCLK_EVENT_IRQ_RESTORE, UINT32_C(0x9C) },
    },
    3u
  ));
  CHECK(manager.sleeping);
  CHECK(mock_pwrclk_clock() == PWRCLK0_CLOCK_SLEEP);

  mock_pwrclk_raise_wake(PWRCLK0_WAKE_RTC | PWRCLK0_WAKE_GPIO);
  offset = mock_pwrclk_event_count();
  CHECK(power_manager_resume(&manager) == POWER_WAKE_GPIO);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_PWRCLK_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0x9C) },
      { MOCK_PWRCLK_EVENT_WAKE_STATUS_READ, PWRCLK0_WAKE_ALL },
      { MOCK_PWRCLK_EVENT_WAKE_ENABLE_WRITE, 0u },
      { MOCK_PWRCLK_EVENT_CLOCK_WRITE, PWRCLK0_CLOCK_RUN },
      { MOCK_PWRCLK_EVENT_WAKE_CLEAR_WRITE,
        PWRCLK0_WAKE_RTC | PWRCLK0_WAKE_GPIO },
      { MOCK_PWRCLK_EVENT_SLEEP_MODE_WRITE, PWRCLK0_SLEEP_NONE },
      { MOCK_PWRCLK_EVENT_IRQ_RESTORE, UINT32_C(0x9C) },
    },
    7u
  ));
  CHECK(!manager.sleeping);
  CHECK(manager.wake_mask == 0u);
  CHECK(mock_pwrclk_clock() == PWRCLK0_CLOCK_RUN);
  CHECK(mock_pwrclk_sleep_mode() == PWRCLK0_SLEEP_NONE);
  CHECK(mock_pwrclk_wake_enable() == 0u);
  CHECK(mock_pwrclk_wake_status() == PWRCLK0_WAKE_UART);
  CHECK(power_manager_clock_hz(&manager) == POWER_RUN_CLOCK_HZ);
  CHECK(mock_pwrclk_irq_state() == UINT32_C(0x9C));
  CHECK(!mock_pwrclk_invalid_access());
  return true;
}

static bool test_uart_idle_wake_and_invalid_resume_are_side_effect_free(void) {
  power_manager_t manager = { 0 };
  power_manager_t uninitialized = { 0 };
  size_t offset;

  mock_pwrclk_reset();
  CHECK(power_manager_resume(NULL) == POWER_WAKE_NONE);
  CHECK(power_manager_resume(&uninitialized) == POWER_WAKE_NONE);
  CHECK(mock_pwrclk_event_count() == 0u);
  CHECK(initialize(&manager));
  CHECK(power_manager_prepare_sleep(
    &manager,
    POWER_SLEEP_MODE_IDLE,
    PWRCLK0_WAKE_UART
  ));
  mock_pwrclk_raise_wake(PWRCLK0_WAKE_UART);
  offset = mock_pwrclk_event_count();
  CHECK(power_manager_resume(&manager) == POWER_WAKE_UART);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_PWRCLK_EVENT_IRQ_SAVE_DISABLE, UINT32_C(1) },
      { MOCK_PWRCLK_EVENT_WAKE_STATUS_READ, PWRCLK0_WAKE_UART },
      { MOCK_PWRCLK_EVENT_WAKE_ENABLE_WRITE, 0u },
      { MOCK_PWRCLK_EVENT_CLOCK_WRITE, PWRCLK0_CLOCK_RUN },
      { MOCK_PWRCLK_EVENT_WAKE_CLEAR_WRITE, PWRCLK0_WAKE_UART },
      { MOCK_PWRCLK_EVENT_SLEEP_MODE_WRITE, PWRCLK0_SLEEP_NONE },
      { MOCK_PWRCLK_EVENT_IRQ_RESTORE, UINT32_C(1) },
    },
    7u
  ));
  CHECK(!manager.sleeping);
  CHECK(!mock_pwrclk_invalid_access());
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "initialization validation and run clock", test_initialization_validation_order_and_clock },
    { "idle wake arming", test_idle_validation_and_ordered_wake_arming },
    { "deep clock and wake priority", test_deep_clock_transition_no_wake_and_priority_resume },
    { "UART wake and invalid resume", test_uart_idle_wake_and_invalid_resume_are_side_effect_free },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) return 1;
    printf("ok - %s\n", tests[index].name);
  }
  return 0;
}

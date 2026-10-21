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
  mock_pwrclk_watch(manager);
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

static bool snapshots_match(
  size_t offset,
  size_t count,
  const power_manager_t *expected,
  uint32_t irq_state
) {
  for (size_t index = offset; index < offset + count; index++) {
    const power_manager_t *snapshot = mock_pwrclk_manager_at(index);
    if (snapshot == NULL || !state_equals(snapshot, expected) ||
        mock_pwrclk_irq_at(index) != irq_state) return false;
  }
  return true;
}

static bool test_complete_mask_status_and_irq_matrix(void) {
  const uint32_t irq_states[] = { 0u, 1u, UINT32_C(0xA5) };
  const power_wake_reason_t reasons[] = {
    POWER_WAKE_NONE, POWER_WAKE_RTC, POWER_WAKE_GPIO, POWER_WAKE_GPIO,
    POWER_WAKE_UART, POWER_WAKE_RTC, POWER_WAKE_GPIO, POWER_WAKE_GPIO,
  };
  for (unsigned mode = 0u; mode < 2u; mode++) {
    for (uint32_t mask = 1u; mask <= PWRCLK0_WAKE_ALL; mask++) {
      if (mode == 1u && (mask & PWRCLK0_WAKE_UART) != 0u) continue;
      for (uint32_t raw = 0u; raw <= PWRCLK0_WAKE_ALL; raw++) {
        for (size_t irq = 0u; irq < 3u; irq++) {
          power_manager_t manager = { 0 };
          power_manager_t before;
          power_manager_t expected;
          size_t offset;
          const uint32_t observed = raw & mask;
          const power_sleep_mode_t sleep_mode = mode == 0u
            ? POWER_SLEEP_MODE_IDLE : POWER_SLEEP_MODE_DEEP;
          const pwrclk0_sleep_t hardware_mode = mode == 0u
            ? PWRCLK0_SLEEP_IDLE : PWRCLK0_SLEEP_DEEP;
          const pwrclk0_clock_t clock = mode == 0u
            ? PWRCLK0_CLOCK_RUN : PWRCLK0_CLOCK_SLEEP;
          mock_pwrclk_reset();
          CHECK(initialize(&manager));
          mock_pwrclk_set_irq_state(irq_states[irq]);
          mock_pwrclk_set_wake_status(PWRCLK0_WAKE_ALL);
          before = manager;
          offset = mock_pwrclk_event_count();
          CHECK(power_manager_prepare_sleep(&manager, sleep_mode, mask));
          expected = before;
          expected.sleeping = true;
          expected.sleep_mode = sleep_mode;
          expected.wake_mask = mask;
          CHECK(state_equals(&manager, &expected));
          CHECK(events_match_from(offset, (const expected_event_t[]) {
            { MOCK_PWRCLK_EVENT_IRQ_SAVE_DISABLE, irq_states[irq] },
            { MOCK_PWRCLK_EVENT_WAKE_ENABLE_WRITE, 0u },
            { MOCK_PWRCLK_EVENT_WAKE_CLEAR_WRITE, PWRCLK0_WAKE_ALL },
            { MOCK_PWRCLK_EVENT_CLOCK_WRITE, (uint32_t)clock },
            { MOCK_PWRCLK_EVENT_WAKE_ENABLE_WRITE, mask },
            { MOCK_PWRCLK_EVENT_SLEEP_MODE_WRITE, (uint32_t)hardware_mode },
            { MOCK_PWRCLK_EVENT_IRQ_RESTORE, irq_states[irq] },
          }, 7u));
          CHECK(snapshots_match(offset, 6u, &before, 0u));
          CHECK(snapshots_match(offset + 6u, 1u, &expected, irq_states[irq]));
          CHECK(mock_pwrclk_wake_status() == 0u);
          CHECK(mock_pwrclk_clock() == clock);
          CHECK(mock_pwrclk_sleep_mode() == hardware_mode);
          CHECK(mock_pwrclk_wake_enable() == mask);
          offset = mock_pwrclk_event_count();
          CHECK(power_manager_clock_hz(&manager) ==
            (mode == 0u ? POWER_RUN_CLOCK_HZ : POWER_SLEEP_CLOCK_HZ));
          CHECK(mock_pwrclk_event_count() == offset);
          CHECK(state_equals(&manager, &expected));

          mock_pwrclk_set_wake_status(raw);
          before = manager;
          CHECK(power_manager_resume(&manager) == reasons[observed]);
          if (observed == 0u) {
            CHECK(events_match_from(offset, (const expected_event_t[]) {
              { MOCK_PWRCLK_EVENT_IRQ_SAVE_DISABLE, irq_states[irq] },
              { MOCK_PWRCLK_EVENT_WAKE_STATUS_READ, raw },
              { MOCK_PWRCLK_EVENT_IRQ_RESTORE, irq_states[irq] },
            }, 3u));
            CHECK(snapshots_match(offset, 2u, &before, 0u));
            CHECK(snapshots_match(offset + 2u, 1u, &before, irq_states[irq]));
            CHECK(mock_pwrclk_clock() == clock);
            CHECK(mock_pwrclk_sleep_mode() == hardware_mode);
            CHECK(mock_pwrclk_wake_enable() == mask);
          } else {
            expected.sleeping = false;
            expected.wake_mask = 0u;
            CHECK(events_match_from(offset, (const expected_event_t[]) {
              { MOCK_PWRCLK_EVENT_IRQ_SAVE_DISABLE, irq_states[irq] },
              { MOCK_PWRCLK_EVENT_WAKE_STATUS_READ, raw },
              { MOCK_PWRCLK_EVENT_WAKE_ENABLE_WRITE, 0u },
              { MOCK_PWRCLK_EVENT_CLOCK_WRITE, PWRCLK0_CLOCK_RUN },
              { MOCK_PWRCLK_EVENT_WAKE_CLEAR_WRITE, observed },
              { MOCK_PWRCLK_EVENT_SLEEP_MODE_WRITE, PWRCLK0_SLEEP_NONE },
              { MOCK_PWRCLK_EVENT_IRQ_RESTORE, irq_states[irq] },
            }, 7u));
            CHECK(snapshots_match(offset, 6u, &before, 0u));
            CHECK(snapshots_match(offset + 6u, 1u, &expected, irq_states[irq]));
            CHECK(mock_pwrclk_clock() == PWRCLK0_CLOCK_RUN);
            CHECK(mock_pwrclk_sleep_mode() == PWRCLK0_SLEEP_NONE);
            CHECK(mock_pwrclk_wake_enable() == 0u);
          }
          CHECK(state_equals(&manager, &expected));
          CHECK(mock_pwrclk_wake_status() == (raw & ~observed));
          CHECK(mock_pwrclk_irq_state() == irq_states[irq]);
          CHECK(!mock_pwrclk_invalid_access());
        }
      }
    }
  }
  return true;
}

static bool test_unusable_managers_are_side_effect_free(void) {
  power_manager_t manager = { 0 };
  mock_pwrclk_reset();
  CHECK(!power_manager_prepare_sleep(NULL, POWER_SLEEP_MODE_IDLE, PWRCLK0_WAKE_RTC));
  CHECK(power_manager_resume(NULL) == POWER_WAKE_NONE);
  CHECK(power_manager_clock_hz(NULL) == 0u);
  for (unsigned variant = 0u; variant < 3u; variant++) {
    manager = (power_manager_t) {
      .registers = variant == 0u ? mock_pwrclk0() : NULL,
      .initialized = variant == 2u,
      .sleeping = true,
      .sleep_mode = POWER_SLEEP_MODE_DEEP,
      .wake_mask = PWRCLK0_WAKE_RTC,
    };
    const power_manager_t before = manager;
    CHECK(!power_manager_prepare_sleep(&manager, POWER_SLEEP_MODE_IDLE, PWRCLK0_WAKE_GPIO));
    CHECK(power_manager_resume(&manager) == POWER_WAKE_NONE);
    CHECK(power_manager_clock_hz(&manager) == 0u);
    CHECK(state_equals(&manager, &before));
  }
  CHECK(mock_pwrclk_event_count() == 0u);
  CHECK(!mock_pwrclk_invalid_access());
  return true;
}

static bool test_all_invalid_masks_and_modes_preserve_state(void) {
  power_manager_t manager = { 0 };
  const power_sleep_mode_t modes[] = {
    (power_sleep_mode_t)-1, POWER_SLEEP_MODE_COUNT, (power_sleep_mode_t)127,
  };
  const uint32_t masks[] = { 0u, UINT32_C(8), UINT32_C(15), UINT32_MAX };
  mock_pwrclk_reset();
  CHECK(initialize(&manager));
  mock_pwrclk_set_wake_status(PWRCLK0_WAKE_ALL);
  mock_pwrclk_set_irq_state(0u);
  const power_manager_t before = manager;
  const size_t offset = mock_pwrclk_event_count();
  for (size_t index = 0u; index < 3u; index++) {
    CHECK(!power_manager_prepare_sleep(&manager, modes[index], PWRCLK0_WAKE_GPIO));
    CHECK(state_equals(&manager, &before));
  }
  for (size_t index = 0u; index < 4u; index++) {
    CHECK(!power_manager_prepare_sleep(&manager, POWER_SLEEP_MODE_IDLE, masks[index]));
    CHECK(state_equals(&manager, &before));
  }
  for (uint32_t mask = PWRCLK0_WAKE_UART; mask <= PWRCLK0_WAKE_ALL; mask++) {
    CHECK(!power_manager_prepare_sleep(&manager, POWER_SLEEP_MODE_DEEP, mask));
    CHECK(state_equals(&manager, &before));
  }
  CHECK(mock_pwrclk_event_count() == offset);
  CHECK(mock_pwrclk_wake_status() == PWRCLK0_WAKE_ALL);
  CHECK(mock_pwrclk_irq_state() == 0u);
  CHECK(!mock_pwrclk_invalid_access());
  return true;
}

static bool test_duplicate_prepare_and_awake_resume(void) {
  power_manager_t manager = { 0 };
  mock_pwrclk_reset();
  CHECK(initialize(&manager));
  power_manager_t before = manager;
  size_t offset = mock_pwrclk_event_count();
  CHECK(power_manager_resume(&manager) == POWER_WAKE_NONE);
  CHECK(state_equals(&manager, &before));
  CHECK(mock_pwrclk_event_count() == offset);
  CHECK(power_manager_prepare_sleep(&manager, POWER_SLEEP_MODE_DEEP, PWRCLK0_WAKE_RTC));
  mock_pwrclk_set_wake_status(PWRCLK0_WAKE_GPIO);
  before = manager;
  offset = mock_pwrclk_event_count();
  CHECK(!power_manager_prepare_sleep(&manager, POWER_SLEEP_MODE_IDLE, PWRCLK0_WAKE_UART));
  CHECK(state_equals(&manager, &before));
  CHECK(mock_pwrclk_event_count() == offset);
  CHECK(mock_pwrclk_wake_status() == PWRCLK0_WAKE_GPIO);
  CHECK(mock_pwrclk_clock() == PWRCLK0_CLOCK_SLEEP);
  CHECK(mock_pwrclk_sleep_mode() == PWRCLK0_SLEEP_DEEP);
  CHECK(mock_pwrclk_wake_enable() == PWRCLK0_WAKE_RTC);
  CHECK(!mock_pwrclk_invalid_access());
  return true;
}

static bool test_reinitialization_replaces_after_hardware_setup(void) {
  power_manager_t manager = { 0 };
  mock_pwrclk_reset();
  CHECK(initialize(&manager));
  CHECK(power_manager_prepare_sleep(&manager, POWER_SLEEP_MODE_DEEP, PWRCLK0_WAKE_GPIO));
  mock_pwrclk_set_irq_state(UINT32_C(0xDA));
  mock_pwrclk_set_wake_status(PWRCLK0_WAKE_ALL);
  const power_manager_t before = manager;
  const size_t offset = mock_pwrclk_event_count();
  CHECK(initialize(&manager));
  CHECK(events_match_from(offset, (const expected_event_t[]) {
    { MOCK_PWRCLK_EVENT_WAKE_ENABLE_WRITE, 0u },
    { MOCK_PWRCLK_EVENT_WAKE_CLEAR_WRITE, PWRCLK0_WAKE_ALL },
    { MOCK_PWRCLK_EVENT_CLOCK_WRITE, PWRCLK0_CLOCK_RUN },
    { MOCK_PWRCLK_EVENT_SLEEP_MODE_WRITE, PWRCLK0_SLEEP_NONE },
  }, 4u));
  CHECK(snapshots_match(offset, 4u, &before, UINT32_C(0xDA)));
  CHECK(state_equals(&manager, &(const power_manager_t) {
    .registers = mock_pwrclk0(), .initialized = true,
    .sleep_mode = POWER_SLEEP_MODE_IDLE,
  }));
  CHECK(mock_pwrclk_clock() == PWRCLK0_CLOCK_RUN);
  CHECK(mock_pwrclk_sleep_mode() == PWRCLK0_SLEEP_NONE);
  CHECK(mock_pwrclk_wake_enable() == 0u);
  CHECK(mock_pwrclk_wake_status() == 0u);
  CHECK(mock_pwrclk_irq_state() == UINT32_C(0xDA));
  CHECK(!mock_pwrclk_invalid_access());
  return true;
}

static bool test_single_snapshot_preserves_later_latches(void) {
  for (unsigned scenario = 0u; scenario < 2u; scenario++) {
    power_manager_t manager = { 0 };
    mock_pwrclk_reset();
    CHECK(initialize(&manager));
    CHECK(power_manager_prepare_sleep(&manager, POWER_SLEEP_MODE_DEEP,
      PWRCLK0_WAKE_RTC | PWRCLK0_WAKE_GPIO));
    mock_pwrclk_set_wake_status(scenario == 0u ? PWRCLK0_WAKE_RTC : 0u);
    mock_pwrclk_latch_after_read(PWRCLK0_WAKE_GPIO);
    const size_t offset = mock_pwrclk_event_count();
    CHECK(power_manager_resume(&manager) ==
      (scenario == 0u ? POWER_WAKE_RTC : POWER_WAKE_NONE));
    CHECK(mock_pwrclk_event_count() == offset + (scenario == 0u ? 7u : 3u));
    CHECK(mock_pwrclk_event_at(offset + 1u) == MOCK_PWRCLK_EVENT_WAKE_STATUS_READ);
    CHECK(mock_pwrclk_wake_status() == PWRCLK0_WAKE_GPIO);
    CHECK(manager.sleeping == (scenario != 0u));
    if (scenario != 0u) CHECK(power_manager_resume(&manager) == POWER_WAKE_GPIO);
    CHECK(!mock_pwrclk_invalid_access());
  }
  return true;
}

static bool test_repeated_transitions_and_pure_clock_queries(void) {
  power_manager_t manager = { 0 };
  mock_pwrclk_reset();
  CHECK(initialize(&manager));
  for (unsigned cycle = 0u; cycle < 4u; cycle++) {
    const power_sleep_mode_t mode = cycle % 2u == 0u
      ? POWER_SLEEP_MODE_DEEP : POWER_SLEEP_MODE_IDLE;
    const uint32_t mask = cycle % 2u == 0u ? PWRCLK0_WAKE_RTC : PWRCLK0_WAKE_UART;
    CHECK(power_manager_prepare_sleep(&manager, mode, mask));
    mock_pwrclk_raise_wake(mask);
    CHECK(power_manager_resume(&manager) ==
      (mode == POWER_SLEEP_MODE_DEEP ? POWER_WAKE_RTC : POWER_WAKE_UART));
    const power_manager_t before = manager;
    const size_t offset = mock_pwrclk_event_count();
    CHECK(power_manager_clock_hz(&manager) == POWER_RUN_CLOCK_HZ);
    CHECK(power_manager_resume(&manager) == POWER_WAKE_NONE);
    CHECK(state_equals(&manager, &before));
    CHECK(mock_pwrclk_event_count() == offset);
  }
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
    { "all masks, status snapshots, and IRQ states", test_complete_mask_status_and_irq_matrix },
    { "unusable managers", test_unusable_managers_are_side_effect_free },
    { "invalid masks and enum values", test_all_invalid_masks_and_modes_preserve_state },
    { "duplicate prepare and awake resume", test_duplicate_prepare_and_awake_resume },
    { "boot reinitialization and state publication", test_reinitialization_replaces_after_hardware_setup },
    { "single snapshot and later latches", test_single_snapshot_preserves_later_latches },
    { "repeated transitions and pure queries", test_repeated_transitions_and_pure_clock_queries },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) return 1;
    printf("ok - %s\n", tests[index].name);
  }
  return 0;
}

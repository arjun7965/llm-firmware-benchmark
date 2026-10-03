#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "brownout_safe_mode.h"
#include "mock_brownout_safe_mode.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
        __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

typedef struct {
  mock_pwr_event_t event;
  uint32_t value;
} expected_event_t;

static uint32_t record_checksum(const brownout_persistent_t *record) {
  return record->magic ^ (uint32_t)record->brownout_count ^
    ((uint32_t)record->safe_mode << 24) ^
    BROWNOUT_PERSISTENT_CHECKSUM_XOR;
}

static bool record_is_valid(const brownout_persistent_t *record) {
  return record->magic == BROWNOUT_PERSISTENT_MAGIC &&
    record->safe_mode <= UINT8_C(1) && record->reserved == 0u &&
    record->checksum == record_checksum(record);
}

static bool manager_equals(
  const brownout_manager_t *left,
  const brownout_manager_t *right
) {
  return left->pwr == right->pwr && left->persistent == right->persistent &&
    left->low_mv == right->low_mv && left->recovery_mv == right->recovery_mv &&
    left->event == right->event && left->initialized == right->initialized;
}

static bool events_match_from(
  size_t offset,
  const expected_event_t *expected,
  size_t expected_count
) {
  if (mock_pwr0_event_count() != offset + expected_count) return false;
  for (size_t index = 0u; index < expected_count; index++) {
    if (
      mock_pwr0_event_at(offset + index) != expected[index].event ||
      mock_pwr0_event_value(offset + index) != expected[index].value
    ) {
      return false;
    }
  }
  return true;
}

static bool initialize(
  brownout_manager_t *manager,
  brownout_persistent_t *persistent
) {
  return brownout_manager_init(
    manager,
    mock_pwr0(),
    persistent,
    UINT16_C(2900),
    UINT16_C(3100)
  );
}

static bool test_validation_and_clean_boot(void) {
  brownout_manager_t manager = {
    .pwr = (volatile pwr0_registers_t *)(uintptr_t)UINT32_C(1),
    .persistent = (brownout_persistent_t *)(uintptr_t)UINT32_C(1),
    .low_mv = UINT16_C(2000),
    .recovery_mv = UINT16_C(2100),
    .event = BROWNOUT_EVENT_RESUMED,
    .initialized = true,
  };
  brownout_persistent_t persistent = {
    .magic = UINT32_C(1),
    .brownout_count = UINT16_C(9),
    .safe_mode = UINT8_C(2),
    .reserved = UINT8_C(7),
    .checksum = UINT32_C(9),
  };
  const brownout_manager_t before_manager = manager;
  const brownout_persistent_t before_persistent = persistent;
  const expected_event_t expected[] = {
    { MOCK_PWR_EVENT_STATUS_READ, 0u },
    { MOCK_PWR_EVENT_SUPPLY_READ, BROWNOUT_MAXIMUM_MV },
    { MOCK_PWR_EVENT_LOAD_WRITE, PWR0_LOAD_ENABLED },
  };

  mock_pwr0_reset();
  CHECK(!brownout_manager_init(
    NULL, mock_pwr0(), &persistent, UINT16_C(2900), UINT16_C(3100)
  ));
  CHECK(!brownout_manager_init(
    &manager, NULL, &persistent, UINT16_C(2900), UINT16_C(3100)
  ));
  CHECK(!brownout_manager_init(
    &manager, mock_pwr0(), NULL, UINT16_C(2900), UINT16_C(3100)
  ));
  CHECK(!brownout_manager_init(
    &manager, mock_pwr0(), &persistent,
    (uint16_t)(BROWNOUT_MINIMUM_MV - UINT16_C(1)), UINT16_C(3100)
  ));
  CHECK(!brownout_manager_init(
    &manager, mock_pwr0(), &persistent, UINT16_C(2900), UINT16_C(2900)
  ));
  CHECK(!brownout_manager_init(
    &manager, mock_pwr0(), &persistent, UINT16_C(2900),
    (uint16_t)(BROWNOUT_MAXIMUM_MV + UINT16_C(1))
  ));
  CHECK(manager_equals(&manager, &before_manager));
  CHECK(persistent.magic == before_persistent.magic);
  CHECK(persistent.brownout_count == before_persistent.brownout_count);
  CHECK(persistent.safe_mode == before_persistent.safe_mode);
  CHECK(persistent.reserved == before_persistent.reserved);
  CHECK(persistent.checksum == before_persistent.checksum);
  CHECK(mock_pwr0_event_count() == 0u);

  CHECK(initialize(&manager, &persistent));
  CHECK(events_match_from(0u, expected, sizeof(expected) / sizeof(expected[0])));
  CHECK(record_is_valid(&persistent));
  CHECK(persistent.brownout_count == 0u);
  CHECK(persistent.safe_mode == 0u);
  CHECK(manager.initialized);
  CHECK(manager.event == BROWNOUT_EVENT_NONE);
  CHECK(mock_pwr0_load_control() == PWR0_LOAD_ENABLED);
  CHECK(!mock_pwr0_invalid_access());
  return true;
}

static bool test_brownout_entry_hysteresis_and_event_gating(void) {
  brownout_manager_t manager = { 0 };
  brownout_persistent_t persistent = { 0 };
  const expected_event_t brownout_events[] = {
    { MOCK_PWR_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xA5) },
    { MOCK_PWR_EVENT_STATUS_READ, PWR0_STATUS_BROWNOUT },
    { MOCK_PWR_EVENT_SUPPLY_READ, UINT16_C(3000) },
    { MOCK_PWR_EVENT_LOAD_WRITE, PWR0_LOAD_SAFE },
    { MOCK_PWR_EVENT_STATUS_CLEAR_WRITE, PWR0_STATUS_BROWNOUT },
    { MOCK_PWR_EVENT_IRQ_RESTORE, UINT32_C(0xA5) },
  };
  const expected_event_t low_recovery_events[] = {
    { MOCK_PWR_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xA5) },
    { MOCK_PWR_EVENT_STATUS_READ, 0u },
    { MOCK_PWR_EVENT_SUPPLY_READ, UINT16_C(3099) },
    { MOCK_PWR_EVENT_LOAD_WRITE, PWR0_LOAD_SAFE },
    { MOCK_PWR_EVENT_IRQ_RESTORE, UINT32_C(0xA5) },
  };
  const expected_event_t resume_events[] = {
    { MOCK_PWR_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xA5) },
    { MOCK_PWR_EVENT_STATUS_READ, 0u },
    { MOCK_PWR_EVENT_SUPPLY_READ, UINT16_C(3100) },
    { MOCK_PWR_EVENT_LOAD_WRITE, PWR0_LOAD_ENABLED },
    { MOCK_PWR_EVENT_IRQ_RESTORE, UINT32_C(0xA5) },
  };
  size_t offset;

  mock_pwr0_reset();
  CHECK(initialize(&manager, &persistent));
  mock_pwr0_set_irq_state(UINT32_C(0xA5));
  mock_pwr0_set_status(PWR0_STATUS_BROWNOUT);
  mock_pwr0_set_supply_mv(UINT16_C(3000));
  offset = mock_pwr0_event_count();
  CHECK(brownout_manager_poll(&manager) == BROWNOUT_EVENT_ENTERED_SAFE_MODE);
  CHECK(events_match_from(
    offset,
    brownout_events,
    sizeof(brownout_events) / sizeof(brownout_events[0])
  ));
  CHECK(persistent.brownout_count == UINT16_C(1));
  CHECK(persistent.safe_mode == UINT8_C(1));
  CHECK(record_is_valid(&persistent));
  CHECK(mock_pwr0_status() == 0u);
  CHECK(mock_pwr0_load_control() == PWR0_LOAD_SAFE);
  CHECK(mock_pwr0_irq_state() == UINT32_C(0xA5));

  offset = mock_pwr0_event_count();
  CHECK(!brownout_manager_resume(&manager));
  CHECK(events_match_from(offset, (const expected_event_t[]) {
    { MOCK_PWR_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xA5) },
    { MOCK_PWR_EVENT_IRQ_RESTORE, UINT32_C(0xA5) },
  }, 2u));
  CHECK(brownout_manager_take_event(&manager) ==
    BROWNOUT_EVENT_ENTERED_SAFE_MODE);

  mock_pwr0_set_supply_mv(UINT16_C(3099));
  offset = mock_pwr0_event_count();
  CHECK(!brownout_manager_resume(&manager));
  CHECK(events_match_from(
    offset,
    low_recovery_events,
    sizeof(low_recovery_events) / sizeof(low_recovery_events[0])
  ));
  CHECK(mock_pwr0_load_control() == PWR0_LOAD_SAFE);

  mock_pwr0_set_supply_mv(UINT16_C(3100));
  offset = mock_pwr0_event_count();
  CHECK(brownout_manager_resume(&manager));
  CHECK(events_match_from(
    offset,
    resume_events,
    sizeof(resume_events) / sizeof(resume_events[0])
  ));
  CHECK(persistent.safe_mode == 0u);
  CHECK(persistent.brownout_count == UINT16_C(1));
  CHECK(manager.event == BROWNOUT_EVENT_RESUMED);
  CHECK(brownout_manager_take_event(&manager) == BROWNOUT_EVENT_RESUMED);
  CHECK(mock_pwr0_load_control() == PWR0_LOAD_ENABLED);
  CHECK(!mock_pwr0_invalid_access());
  return true;
}

static bool test_persistent_safe_boot_and_saturating_counter(void) {
  brownout_manager_t manager = { 0 };
  brownout_manager_t rebooted = { 0 };
  brownout_persistent_t persistent = { 0 };
  brownout_persistent_t saturated = {
    .magic = BROWNOUT_PERSISTENT_MAGIC,
    .brownout_count = UINT16_MAX,
    .safe_mode = 0u,
    .reserved = 0u,
    .checksum = 0u,
  };

  mock_pwr0_reset();
  CHECK(initialize(&manager, &persistent));
  mock_pwr0_set_status(PWR0_STATUS_BROWNOUT);
  CHECK(brownout_manager_poll(&manager) == BROWNOUT_EVENT_ENTERED_SAFE_MODE);
  CHECK(brownout_manager_take_event(&manager) ==
    BROWNOUT_EVENT_ENTERED_SAFE_MODE);
  mock_pwr0_set_status(0u);
  mock_pwr0_set_supply_mv(BROWNOUT_MAXIMUM_MV);
  CHECK(initialize(&rebooted, &persistent));
  CHECK(rebooted.event == BROWNOUT_EVENT_NONE);
  CHECK(mock_pwr0_load_control() == PWR0_LOAD_SAFE);
  CHECK(brownout_manager_resume(&rebooted));
  CHECK(persistent.safe_mode == 0u);

  saturated.checksum = record_checksum(&saturated);
  mock_pwr0_reset();
  mock_pwr0_set_status(PWR0_STATUS_BROWNOUT);
  CHECK(initialize(&manager, &saturated));
  CHECK(saturated.brownout_count == UINT16_MAX);
  CHECK(saturated.safe_mode == UINT8_C(1));
  CHECK(record_is_valid(&saturated));
  CHECK(!mock_pwr0_invalid_access());
  return true;
}

static bool test_invalid_foreground_calls_have_no_side_effects(void) {
  brownout_manager_t manager = { 0 };
  const brownout_manager_t before = manager;

  mock_pwr0_reset();
  CHECK(brownout_manager_poll(NULL) == BROWNOUT_EVENT_NONE);
  CHECK(brownout_manager_poll(&manager) == BROWNOUT_EVENT_NONE);
  CHECK(!brownout_manager_resume(NULL));
  CHECK(!brownout_manager_resume(&manager));
  CHECK(brownout_manager_take_event(NULL) == BROWNOUT_EVENT_NONE);
  CHECK(brownout_manager_take_event(&manager) == BROWNOUT_EVENT_NONE);
  CHECK(manager_equals(&manager, &before));
  CHECK(mock_pwr0_event_count() == 0u);
  CHECK(!mock_pwr0_invalid_access());
  return true;
}

static brownout_persistent_t valid_record(uint16_t count, uint8_t safe) {
  brownout_persistent_t record = {
    .magic = BROWNOUT_PERSISTENT_MAGIC,
    .brownout_count = count,
    .safe_mode = safe,
  };
  record.checksum = record_checksum(&record);
  return record;
}

static bool test_individual_retained_corruption(void) {
  for (unsigned field = 0u; field < 5u; field++) {
    for (unsigned active = 0u; active < 2u; active++) {
      brownout_manager_t manager = { 0 };
      brownout_persistent_t record = valid_record(UINT16_C(17), UINT8_C(1));
      brownout_persistent_t observed;
      switch (field) {
        case 0u: record.magic ^= UINT32_C(1); break;
        case 1u: record.safe_mode = UINT8_C(2); break;
        case 2u: record.reserved = UINT8_C(1); break;
        case 3u: record.brownout_count ^= UINT16_C(1); break;
        default: record.checksum ^= UINT32_C(1); break;
      }
      /* Isolate structural checks from the checksum check. */
      if (field < 3u) record.checksum = record_checksum(&record);
      mock_pwr0_reset();
      mock_pwr0_watch_persistent(&record);
      if (active != 0u) mock_pwr0_set_status(PWR0_STATUS_BROWNOUT);
      CHECK(initialize(&manager, &record));
      observed = mock_pwr0_persistent_at(0u);
      CHECK(record_is_valid(&observed));
      CHECK(observed.brownout_count == 0u && observed.safe_mode == 0u);
      CHECK(record_is_valid(&record));
      CHECK(record.brownout_count == active && record.safe_mode == active);
      CHECK(manager.event == (active != 0u
        ? BROWNOUT_EVENT_ENTERED_SAFE_MODE : BROWNOUT_EVENT_NONE));
      CHECK(!mock_pwr0_invalid_access());
    }
  }
  return true;
}

static bool test_thresholds_and_latched_resume(void) {
  const uint16_t thresholds[][2] = {
    { BROWNOUT_MINIMUM_MV, BROWNOUT_MINIMUM_MV + UINT16_C(1) },
    { BROWNOUT_MAXIMUM_MV - UINT16_C(1), BROWNOUT_MAXIMUM_MV },
  };
  for (size_t index = 0u; index < 2u; index++) {
    brownout_manager_t manager = { 0 };
    brownout_persistent_t record = valid_record(UINT16_C(6), 0u);
    size_t offset;
    mock_pwr0_reset();
    CHECK(brownout_manager_init(&manager, mock_pwr0(), &record,
      thresholds[index][0], thresholds[index][1]));
    CHECK(record.brownout_count == UINT16_C(6));
    mock_pwr0_set_irq_state(0u);
    mock_pwr0_set_supply_mv(thresholds[index][0] + UINT16_C(1));
    CHECK(brownout_manager_poll(&manager) == BROWNOUT_EVENT_NONE);
    CHECK(record.safe_mode == 0u);
    mock_pwr0_set_supply_mv(thresholds[index][0]);
    offset = mock_pwr0_event_count();
    CHECK(brownout_manager_poll(&manager) == BROWNOUT_EVENT_ENTERED_SAFE_MODE);
    CHECK(events_match_from(offset, (const expected_event_t[]) {
      { MOCK_PWR_EVENT_IRQ_SAVE_DISABLE, 0u },
      { MOCK_PWR_EVENT_STATUS_READ, 0u },
      { MOCK_PWR_EVENT_SUPPLY_READ, thresholds[index][0] },
      { MOCK_PWR_EVENT_LOAD_WRITE, PWR0_LOAD_SAFE },
      { MOCK_PWR_EVENT_IRQ_RESTORE, 0u },
    }, 5u));
    CHECK(record.brownout_count == UINT16_C(7));
    CHECK(brownout_manager_take_event(&manager) == BROWNOUT_EVENT_ENTERED_SAFE_MODE);
    CHECK(brownout_manager_take_event(&manager) == BROWNOUT_EVENT_NONE);
    mock_pwr0_set_supply_mv(thresholds[index][1]);
    mock_pwr0_set_status(PWR0_STATUS_BROWNOUT);
    offset = mock_pwr0_event_count();
    CHECK(!brownout_manager_resume(&manager));
    CHECK(events_match_from(offset, (const expected_event_t[]) {
      { MOCK_PWR_EVENT_IRQ_SAVE_DISABLE, 0u },
      { MOCK_PWR_EVENT_STATUS_READ, PWR0_STATUS_BROWNOUT },
      { MOCK_PWR_EVENT_SUPPLY_READ, thresholds[index][1] },
      { MOCK_PWR_EVENT_LOAD_WRITE, PWR0_LOAD_SAFE },
      { MOCK_PWR_EVENT_IRQ_RESTORE, 0u },
    }, 5u));
    CHECK(mock_pwr0_status() == PWR0_STATUS_BROWNOUT);
    CHECK(record.safe_mode == UINT8_C(1));
    CHECK(manager.event == BROWNOUT_EVENT_NONE);
    CHECK(brownout_manager_poll(&manager) == BROWNOUT_EVENT_NONE);
    CHECK(record.brownout_count == UINT16_C(7));
    CHECK(brownout_manager_resume(&manager));
    CHECK(record_is_valid(&record));
    CHECK(mock_pwr0_irq_state() == 0u);
    CHECK(!mock_pwr0_invalid_access());

    /* Voltage alone must also cause active boot at the inclusive low boundary. */
    mock_pwr0_reset();
    record = valid_record(0u, 0u);
    mock_pwr0_set_supply_mv(thresholds[index][0]);
    CHECK(brownout_manager_init(&manager, mock_pwr0(), &record,
      thresholds[index][0], thresholds[index][1]));
    CHECK(record.safe_mode == UINT8_C(1) && record.brownout_count == UINT16_C(1));
    CHECK(manager.event == BROWNOUT_EVENT_ENTERED_SAFE_MODE);
    CHECK(!mock_pwr0_invalid_access());
  }
  return true;
}

static bool test_retained_order_and_repeated_resets(void) {
  brownout_manager_t manager = { 0 };
  brownout_persistent_t record = valid_record(UINT16_MAX - UINT16_C(1), 0u);
  brownout_persistent_t observed;
  size_t offset;
  mock_pwr0_reset();
  mock_pwr0_watch_persistent(&record);
  mock_pwr0_set_status(PWR0_STATUS_BROWNOUT);
  CHECK(initialize(&manager, &record));
  observed = mock_pwr0_persistent_at(2u); /* SAFE write precedes retaining entry. */
  CHECK(observed.safe_mode == 0u && observed.brownout_count == UINT16_MAX - UINT16_C(1));
  observed = mock_pwr0_persistent_at(3u); /* Latch clear sees complete safe record. */
  CHECK(record_is_valid(&observed));
  CHECK(observed.safe_mode == UINT8_C(1) && observed.brownout_count == UINT16_MAX);

  for (unsigned reboot = 0u; reboot < 3u; reboot++) {
    mock_pwr0_reset();
    mock_pwr0_watch_persistent(&record);
    mock_pwr0_set_status(PWR0_STATUS_BROWNOUT);
    CHECK(initialize(&manager, &record));
    CHECK(record.brownout_count == UINT16_MAX);
    CHECK(manager.event == BROWNOUT_EVENT_ENTERED_SAFE_MODE);
    CHECK(mock_pwr0_load_control() == PWR0_LOAD_SAFE);
    CHECK(record_is_valid(&record));
    CHECK(!mock_pwr0_invalid_access());
  }
  mock_pwr0_reset();
  mock_pwr0_watch_persistent(&record);
  CHECK(initialize(&manager, &record));
  CHECK(manager.event == BROWNOUT_EVENT_NONE);
  CHECK(mock_pwr0_load_control() == PWR0_LOAD_SAFE);
  offset = mock_pwr0_event_count();
  CHECK(brownout_manager_resume(&manager));
  observed = mock_pwr0_persistent_at(offset + 3u);
  CHECK(record_is_valid(&observed) && observed.safe_mode == UINT8_C(1));
  observed = mock_pwr0_persistent_at(offset + 4u);
  CHECK(record_is_valid(&observed) && observed.safe_mode == 0u);

  /* A new entry replaces RESUMED; repeated/healthy polls retain pending entry. */
  mock_pwr0_set_status(PWR0_STATUS_BROWNOUT);
  offset = mock_pwr0_event_count();
  CHECK(brownout_manager_poll(&manager) == BROWNOUT_EVENT_ENTERED_SAFE_MODE);
  observed = mock_pwr0_persistent_at(offset + 3u);
  CHECK(observed.safe_mode == 0u);
  observed = mock_pwr0_persistent_at(offset + 4u);
  CHECK(record_is_valid(&observed) && observed.safe_mode == UINT8_C(1));
  CHECK(record.brownout_count == UINT16_MAX);
  mock_pwr0_set_supply_mv(UINT16_C(2900));
  offset = mock_pwr0_event_count();
  CHECK(brownout_manager_poll(&manager) == BROWNOUT_EVENT_NONE);
  CHECK(events_match_from(offset, (const expected_event_t[]) {
    { MOCK_PWR_EVENT_IRQ_SAVE_DISABLE, UINT32_C(1) },
    { MOCK_PWR_EVENT_STATUS_READ, 0u },
    { MOCK_PWR_EVENT_SUPPLY_READ, UINT16_C(2900) },
    { MOCK_PWR_EVENT_LOAD_WRITE, PWR0_LOAD_SAFE },
    { MOCK_PWR_EVENT_IRQ_RESTORE, UINT32_C(1) },
  }, 5u));
  mock_pwr0_set_supply_mv(BROWNOUT_MAXIMUM_MV);
  CHECK(brownout_manager_poll(&manager) == BROWNOUT_EVENT_NONE);
  CHECK(manager.event == BROWNOUT_EVENT_ENTERED_SAFE_MODE);
  CHECK(mock_pwr0_load_control() == PWR0_LOAD_SAFE);
  CHECK(!brownout_manager_resume(&manager));
  CHECK(brownout_manager_take_event(&manager) == BROWNOUT_EVENT_ENTERED_SAFE_MODE);
  CHECK(brownout_manager_take_event(&manager) == BROWNOUT_EVENT_NONE);
  CHECK(brownout_manager_resume(&manager));
  CHECK(brownout_manager_poll(&manager) == BROWNOUT_EVENT_NONE);
  CHECK(manager.event == BROWNOUT_EVENT_RESUMED);
  offset = mock_pwr0_event_count();
  CHECK(!brownout_manager_resume(&manager));
  CHECK(events_match_from(offset, (const expected_event_t[]) {
    { MOCK_PWR_EVENT_IRQ_SAVE_DISABLE, UINT32_C(1) },
    { MOCK_PWR_EVENT_IRQ_RESTORE, UINT32_C(1) },
  }, 2u));
  CHECK(!mock_pwr0_invalid_access());
  return true;
}

static bool test_partial_managers_and_invalid_bounds(void) {
  brownout_persistent_t record = valid_record(UINT16_C(8), UINT8_C(1));
  brownout_manager_t manager = { 0 };
  const uint16_t invalid[][2] = {
    { BROWNOUT_MAXIMUM_MV, BROWNOUT_MAXIMUM_MV },
    { UINT16_MAX, UINT16_MAX },
    { UINT16_C(2900), UINT16_C(2899) },
    { 0u, BROWNOUT_MAXIMUM_MV },
  };
  mock_pwr0_reset();
  for (size_t index = 0u; index < 4u; index++) {
    CHECK(!brownout_manager_init(&manager, mock_pwr0(), &record,
      invalid[index][0], invalid[index][1]));
    CHECK(!manager.initialized);
    CHECK(record_is_valid(&record));
    CHECK(record.brownout_count == UINT16_C(8) && record.safe_mode == UINT8_C(1));
  }
  for (unsigned missing = 0u; missing < 2u; missing++) {
    manager = (brownout_manager_t) {
      .initialized = true,
      .pwr = missing == 0u ? NULL : mock_pwr0(),
      .persistent = missing == 1u ? NULL : &record,
      .event = BROWNOUT_EVENT_RESUMED,
    };
    const brownout_manager_t before = manager;
    CHECK(brownout_manager_poll(&manager) == BROWNOUT_EVENT_NONE);
    CHECK(!brownout_manager_resume(&manager));
    CHECK(brownout_manager_take_event(&manager) == BROWNOUT_EVENT_NONE);
    CHECK(manager_equals(&manager, &before));
  }
  CHECK(mock_pwr0_event_count() == 0u);
  CHECK(!mock_pwr0_invalid_access());
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "validation and clean boot", test_validation_and_clean_boot },
    { "brownout entry and hysteresis", test_brownout_entry_hysteresis_and_event_gating },
    { "persistent safe boot", test_persistent_safe_boot_and_saturating_counter },
    { "invalid foreground calls", test_invalid_foreground_calls_have_no_side_effects },
    { "individual retained corruption", test_individual_retained_corruption },
    { "thresholds and latched resume", test_thresholds_and_latched_resume },
    { "retained ordering and repeated resets", test_retained_order_and_repeated_resets },
    { "partial managers and invalid bounds", test_partial_managers_and_invalid_bounds },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) {
      fprintf(stderr, "failed: %s\n", tests[index].name);
      return 1;
    }
  }

  printf("Brownout safe-mode public tests passed (%zu tests).\n",
    sizeof(tests) / sizeof(tests[0]));
  return 0;
}

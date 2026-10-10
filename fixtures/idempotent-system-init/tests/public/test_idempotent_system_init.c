#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "idempotent_system_init.h"
#include "mock_idempotent_system_init.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
        __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

typedef struct {
  mock_system_event_t event;
  uint32_t value;
} expected_event_t;

static const system_init_config_t valid_config = {
  .clock_hz = UINT32_C(48000000),
  .peripheral_mask = UINT32_C(3),
};

static uint32_t config_signature(const system_init_config_t *config) {
  return config->clock_hz ^ (config->peripheral_mask << 16) ^
    SYSTEM_INIT_SIGNATURE_XOR;
}

static uint32_t persistent_checksum(const system_init_persistent_t *persistent) {
  return persistent->magic ^ persistent->config_signature ^
    (uint32_t)persistent->successful_boots ^
    ((uint32_t)persistent->safe_mode << 24) ^
    SYSTEM_INIT_PERSISTENT_CHECKSUM_XOR;
}

static bool persistent_is_valid(const system_init_persistent_t *persistent) {
  return persistent->magic == SYSTEM_INIT_PERSISTENT_MAGIC &&
    persistent->safe_mode <= UINT8_C(1) && persistent->reserved == 0u &&
    persistent->checksum == persistent_checksum(persistent);
}

static bool state_equals(const system_init_t *left, const system_init_t *right) {
  return left->system == right->system && left->persistent == right->persistent &&
    left->config.clock_hz == right->config.clock_hz &&
    left->config.peripheral_mask == right->config.peripheral_mask &&
    left->event == right->event && left->safe_mode == right->safe_mode &&
    left->initialized == right->initialized;
}

static bool persistent_equals(
  const system_init_persistent_t *left,
  const system_init_persistent_t *right
) {
  return left->magic == right->magic &&
    left->config_signature == right->config_signature &&
    left->successful_boots == right->successful_boots &&
    left->safe_mode == right->safe_mode && left->reserved == right->reserved &&
    left->checksum == right->checksum;
}

static system_init_persistent_t retained_record(uint16_t boots, bool safe) {
  system_init_persistent_t record = {
    .magic = SYSTEM_INIT_PERSISTENT_MAGIC,
    .config_signature = config_signature(&valid_config),
    .successful_boots = boots,
    .safe_mode = safe ? UINT8_C(1) : UINT8_C(0),
    .reserved = 0u,
    .checksum = 0u,
  };
  record.checksum = persistent_checksum(&record);
  return record;
}

static bool events_match_from(
  size_t offset,
  const expected_event_t *expected,
  size_t expected_count
) {
  if (mock_system0_event_count() != offset + expected_count) return false;
  for (size_t index = 0u; index < expected_count; index++) {
    if (
      mock_system0_event_at(offset + index) != expected[index].event ||
      mock_system0_event_value(offset + index) != expected[index].value
    ) {
      return false;
    }
  }
  return true;
}

static system_init_result_t initialize(
  system_init_t *state,
  system_init_persistent_t *persistent
) {
  return system_init_initialize(
    state,
    mock_system0(),
    persistent,
    &valid_config
  );
}

static bool test_validation_initialization_and_idempotency(void) {
  system_init_t state = { 0 };
  system_init_persistent_t persistent = {
    .magic = UINT32_C(7),
    .config_signature = UINT32_C(8),
    .successful_boots = UINT16_C(9),
    .safe_mode = UINT8_C(2),
    .reserved = UINT8_C(3),
    .checksum = UINT32_C(4),
  };
  const system_init_t before_state = state;
  const system_init_persistent_t before_persistent = persistent;
  const system_init_config_t bad_clock = {
    .clock_hz = UINT32_C(12345678),
    .peripheral_mask = UINT32_C(1),
  };
  const system_init_config_t bad_mask = {
    .clock_hz = UINT32_C(48000000),
    .peripheral_mask = UINT32_C(0x10),
  };
  const expected_event_t initial_events[] = {
    { MOCK_SYSTEM_EVENT_CONTROL_WRITE, SYSTEM0_CONTROL_SAFE },
    { MOCK_SYSTEM_EVENT_CLOCK_WRITE, UINT32_C(48000000) },
    { MOCK_SYSTEM_EVENT_PERIPHERAL_MASK_WRITE, UINT32_C(3) },
    { MOCK_SYSTEM_EVENT_CONTROL_WRITE, SYSTEM0_CONTROL_READY },
  };
  const system_init_config_t conflicting = {
    .clock_hz = UINT32_C(48000000),
    .peripheral_mask = UINT32_C(7),
  };
  size_t offset;

  mock_system0_reset();
  CHECK(system_init_initialize(NULL, mock_system0(), &persistent, &valid_config) ==
    SYSTEM_INIT_RESULT_INVALID);
  CHECK(system_init_initialize(&state, NULL, &persistent, &valid_config) ==
    SYSTEM_INIT_RESULT_INVALID);
  CHECK(system_init_initialize(&state, mock_system0(), NULL, &valid_config) ==
    SYSTEM_INIT_RESULT_INVALID);
  CHECK(system_init_initialize(&state, mock_system0(), &persistent, NULL) ==
    SYSTEM_INIT_RESULT_INVALID);
  CHECK(system_init_initialize(&state, mock_system0(), &persistent, &bad_clock) ==
    SYSTEM_INIT_RESULT_INVALID);
  CHECK(system_init_initialize(&state, mock_system0(), &persistent, &bad_mask) ==
    SYSTEM_INIT_RESULT_INVALID);
  CHECK(state_equals(&state, &before_state));
  CHECK(persistent_equals(&persistent, &before_persistent));
  CHECK(mock_system0_event_count() == 0u);

  CHECK(initialize(&state, &persistent) == SYSTEM_INIT_RESULT_CONFIGURED);
  CHECK(events_match_from(
    0u,
    initial_events,
    sizeof(initial_events) / sizeof(initial_events[0])
  ));
  CHECK(state.initialized);
  CHECK(!state.safe_mode);
  CHECK(state.event == SYSTEM_INIT_EVENT_INITIALIZED);
  CHECK(persistent_is_valid(&persistent));
  CHECK(persistent.successful_boots == UINT16_C(1));
  CHECK(persistent.safe_mode == 0u);
  CHECK(mock_system0_control() == SYSTEM0_CONTROL_READY);

  offset = mock_system0_event_count();
  CHECK(initialize(&state, &persistent) == SYSTEM_INIT_RESULT_ALREADY_READY);
  CHECK(mock_system0_event_count() == offset);
  CHECK(system_init_initialize(
    &state, mock_system0(), &persistent, &conflicting
  ) == SYSTEM_INIT_RESULT_CONFLICT);
  CHECK(mock_system0_event_count() == offset);
  CHECK(system_init_take_event(&state) == SYSTEM_INIT_EVENT_INITIALIZED);
  CHECK(!mock_system0_invalid_access());
  return true;
}

static bool test_explicit_safe_mode_and_resume(void) {
  system_init_t state = { 0 };
  system_init_persistent_t persistent = { 0 };
  const expected_event_t enter_events[] = {
    { MOCK_SYSTEM_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xA5) },
    { MOCK_SYSTEM_EVENT_CONTROL_WRITE, SYSTEM0_CONTROL_SAFE },
    { MOCK_SYSTEM_EVENT_IRQ_RESTORE, UINT32_C(0xA5) },
  };
  const expected_event_t resume_events[] = {
    { MOCK_SYSTEM_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xA5) },
    { MOCK_SYSTEM_EVENT_CONTROL_WRITE, SYSTEM0_CONTROL_SAFE },
    { MOCK_SYSTEM_EVENT_CLOCK_WRITE, UINT32_C(48000000) },
    { MOCK_SYSTEM_EVENT_PERIPHERAL_MASK_WRITE, UINT32_C(3) },
    { MOCK_SYSTEM_EVENT_CONTROL_WRITE, SYSTEM0_CONTROL_READY },
    { MOCK_SYSTEM_EVENT_IRQ_RESTORE, UINT32_C(0xA5) },
  };
  size_t offset;

  mock_system0_reset();
  CHECK(initialize(&state, &persistent) == SYSTEM_INIT_RESULT_CONFIGURED);
  CHECK(system_init_take_event(&state) == SYSTEM_INIT_EVENT_INITIALIZED);
  mock_system0_set_irq_state(UINT32_C(0xA5));
  offset = mock_system0_event_count();
  CHECK(system_init_enter_safe_mode(&state));
  CHECK(events_match_from(
    offset,
    enter_events,
    sizeof(enter_events) / sizeof(enter_events[0])
  ));
  CHECK(state.safe_mode);
  CHECK(persistent.safe_mode == UINT8_C(1));
  CHECK(initialize(&state, &persistent) == SYSTEM_INIT_RESULT_SAFE_MODE_LATCHED);

  offset = mock_system0_event_count();
  CHECK(!system_init_resume(&state));
  CHECK(events_match_from(offset, (const expected_event_t[]) {
    { MOCK_SYSTEM_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xA5) },
    { MOCK_SYSTEM_EVENT_IRQ_RESTORE, UINT32_C(0xA5) },
  }, 2u));
  CHECK(system_init_take_event(&state) == SYSTEM_INIT_EVENT_ENTERED_SAFE_MODE);
  offset = mock_system0_event_count();
  CHECK(system_init_resume(&state));
  CHECK(events_match_from(
    offset,
    resume_events,
    sizeof(resume_events) / sizeof(resume_events[0])
  ));
  CHECK(!state.safe_mode);
  CHECK(persistent.safe_mode == 0u);
  CHECK(system_init_take_event(&state) == SYSTEM_INIT_EVENT_RESUMED);
  CHECK(mock_system0_irq_state() == UINT32_C(0xA5));
  CHECK(!mock_system0_invalid_access());
  return true;
}

static bool test_retained_safe_boot_integrity_and_saturation(void) {
  system_init_t state = { 0 };
  system_init_t saturated_state = { 0 };
  system_init_persistent_t persistent = {
    .magic = SYSTEM_INIT_PERSISTENT_MAGIC,
    .config_signature = UINT32_C(0),
    .successful_boots = UINT16_C(9),
    .safe_mode = UINT8_C(1),
    .reserved = 0u,
    .checksum = 0u,
  };
  system_init_persistent_t saturated = {
    .magic = SYSTEM_INIT_PERSISTENT_MAGIC,
    .config_signature = UINT32_C(0),
    .successful_boots = UINT16_MAX,
    .safe_mode = 0u,
    .reserved = 0u,
    .checksum = 0u,
  };

  persistent.config_signature = config_signature(&valid_config);
  persistent.checksum = persistent_checksum(&persistent);
  mock_system0_reset();
  CHECK(initialize(&state, &persistent) == SYSTEM_INIT_RESULT_SAFE_MODE_LATCHED);
  CHECK(mock_system0_control() == SYSTEM0_CONTROL_SAFE);
  CHECK(state.safe_mode);
  CHECK(system_init_take_event(&state) == SYSTEM_INIT_EVENT_ENTERED_SAFE_MODE);
  persistent.checksum ^= UINT32_C(1);
  CHECK(!system_init_resume(&state));
  CHECK(mock_system0_control() == SYSTEM0_CONTROL_SAFE);
  CHECK(state.safe_mode);

  saturated.config_signature = config_signature(&valid_config);
  saturated.checksum = persistent_checksum(&saturated);
  mock_system0_reset();
  CHECK(initialize(&saturated_state, &saturated) == SYSTEM_INIT_RESULT_CONFIGURED);
  CHECK(saturated.successful_boots == UINT16_MAX);
  CHECK(persistent_is_valid(&saturated));
  CHECK(!mock_system0_invalid_access());
  return true;
}

static bool test_invalid_foreground_calls_have_no_side_effects(void) {
  system_init_t state = { 0 };
  const system_init_t before = state;

  mock_system0_reset();
  CHECK(!system_init_enter_safe_mode(NULL));
  CHECK(!system_init_enter_safe_mode(&state));
  CHECK(!system_init_resume(NULL));
  CHECK(!system_init_resume(&state));
  CHECK(system_init_take_event(NULL) == SYSTEM_INIT_EVENT_NONE);
  CHECK(system_init_take_event(&state) == SYSTEM_INIT_EVENT_NONE);
  CHECK(state_equals(&state, &before));
  CHECK(mock_system0_event_count() == 0u);
  CHECK(!mock_system0_invalid_access());
  return true;
}


static bool test_clock_and_mask_boundaries(void) {
  const uint32_t bad_clocks[] = { 0u, 7000000u, 121000000u, 8000001u, UINT32_MAX };
  const uint32_t bad_masks[] = { 0u, 16u, UINT32_MAX };
  const uint32_t clocks[] = { SYSTEM0_MIN_CLOCK_HZ, SYSTEM0_MAX_CLOCK_HZ };
  const uint32_t masks[] = { 1u, SYSTEM0_PERIPHERAL_MASK_ALL };
  for (size_t index = 0; index < sizeof(bad_clocks) / sizeof(bad_clocks[0]); index++) {
    system_init_t state = { 0 };
    system_init_persistent_t record = retained_record(7u, true);
    const system_init_persistent_t before = record;
    system_init_config_t config = valid_config;
    config.clock_hz = bad_clocks[index];
    mock_system0_reset();
    CHECK(system_init_initialize(&state, mock_system0(), &record, &config) ==
      SYSTEM_INIT_RESULT_INVALID);
    CHECK(!state.initialized && persistent_equals(&record, &before));
    CHECK(mock_system0_event_count() == 0u && !mock_system0_invalid_access());
  }
  for (size_t index = 0; index < sizeof(bad_masks) / sizeof(bad_masks[0]); index++) {
    system_init_t state = { 0 };
    system_init_persistent_t record = retained_record(7u, true);
    const system_init_persistent_t before = record;
    system_init_config_t config = valid_config;
    config.peripheral_mask = bad_masks[index];
    mock_system0_reset();
    CHECK(system_init_initialize(&state, mock_system0(), &record, &config) ==
      SYSTEM_INIT_RESULT_INVALID);
    CHECK(!state.initialized && persistent_equals(&record, &before));
    CHECK(mock_system0_event_count() == 0u && !mock_system0_invalid_access());
  }
  for (size_t c = 0; c < 2u; c++) {
    for (size_t m = 0; m < 2u; m++) {
      system_init_t state = { 0 };
      system_init_persistent_t record = { 0 };
      const system_init_config_t config = { clocks[c], masks[m] };
      const expected_event_t events[] = {
        { MOCK_SYSTEM_EVENT_CONTROL_WRITE, SYSTEM0_CONTROL_SAFE },
        { MOCK_SYSTEM_EVENT_CLOCK_WRITE, clocks[c] },
        { MOCK_SYSTEM_EVENT_PERIPHERAL_MASK_WRITE, masks[m] },
        { MOCK_SYSTEM_EVENT_CONTROL_WRITE, SYSTEM0_CONTROL_READY },
      };
      mock_system0_reset();
      mock_system0_set_irq_state(0u);
      CHECK(system_init_initialize(&state, mock_system0(), &record, &config) ==
        SYSTEM_INIT_RESULT_CONFIGURED);
      CHECK(events_match_from(0u, events, 4u));
      CHECK(state.config.clock_hz == clocks[c] && state.config.peripheral_mask == masks[m]);
      CHECK(record.config_signature == config_signature(&config));
      CHECK(record.successful_boots == 1u && persistent_is_valid(&record));
      CHECK(mock_system0_irq_state() == 0u && !mock_system0_invalid_access());
    }
  }
  return true;
}

static bool test_repeat_and_conflict_preserve_every_field(void) {
  system_init_t state = { 0 };
  system_init_persistent_t record = retained_record(7u, false);
  system_init_persistent_t other_record = retained_record(19u, true);
  const system_init_persistent_t other_before = other_record;
  system_init_config_t config = valid_config;
  mock_system0_reset();
  CHECK(initialize(&state, &record) == SYSTEM_INIT_RESULT_CONFIGURED);
  CHECK(record.successful_boots == 8u);
  const system_init_t before = state;
  const system_init_persistent_t record_before = record;
  const size_t offset = mock_system0_event_count();
  CHECK(initialize(&state, &record) == SYSTEM_INIT_RESULT_ALREADY_READY);
  CHECK(state_equals(&state, &before) && persistent_equals(&record, &record_before));
  CHECK(system_init_initialize(&state, mock_system0(), &other_record, &config) ==
    SYSTEM_INIT_RESULT_CONFLICT);
  /* Only pointer comparison is permitted for a different opaque controller. */
  CHECK(system_init_initialize(&state, (volatile system0_registers_t *)&other_record,
    &record, &config) == SYSTEM_INIT_RESULT_CONFLICT);
  config.clock_hz = SYSTEM0_MIN_CLOCK_HZ;
  CHECK(system_init_initialize(&state, mock_system0(), &record, &config) ==
    SYSTEM_INIT_RESULT_CONFLICT);
  config = valid_config;
  config.peripheral_mask = 7u;
  CHECK(system_init_initialize(&state, mock_system0(), &record, &config) ==
    SYSTEM_INIT_RESULT_CONFLICT);
  config.peripheral_mask = 0u;
  CHECK(system_init_initialize(&state, mock_system0(), &record, &config) ==
    SYSTEM_INIT_RESULT_INVALID);
  CHECK(state_equals(&state, &before));
  CHECK(persistent_equals(&record, &record_before));
  CHECK(persistent_equals(&other_record, &other_before));
  CHECK(mock_system0_event_count() == offset && !mock_system0_invalid_access());
  return true;
}

static bool test_retained_safe_boot_with_new_configuration(void) {
  system_init_t state = { 0 };
  system_init_persistent_t record = retained_record(23u, true);
  const system_init_persistent_t before = record;
  const system_init_config_t config = { SYSTEM0_MIN_CLOCK_HZ, 15u };
  mock_system0_reset();
  CHECK(system_init_initialize(&state, mock_system0(), &record, &config) ==
    SYSTEM_INIT_RESULT_SAFE_MODE_LATCHED);
  CHECK(events_match_from(0u, (const expected_event_t[]) {
    { MOCK_SYSTEM_EVENT_CONTROL_WRITE, SYSTEM0_CONTROL_SAFE },
  }, 1u));
  CHECK(persistent_equals(&record, &before));
  CHECK(state.system == mock_system0() && state.persistent == &record);
  CHECK(state.initialized && state.safe_mode);
  CHECK(state.config.clock_hz == config.clock_hz &&
    state.config.peripheral_mask == config.peripheral_mask);
  const system_init_t state_before = state;
  const size_t offset = mock_system0_event_count();
  CHECK(system_init_initialize(&state, mock_system0(), &record, &config) ==
    SYSTEM_INIT_RESULT_SAFE_MODE_LATCHED);
  CHECK(state_equals(&state, &state_before) && persistent_equals(&record, &before));
  CHECK(mock_system0_event_count() == offset);
  CHECK(system_init_take_event(&state) == SYSTEM_INIT_EVENT_ENTERED_SAFE_MODE);
  CHECK(system_init_resume(&state));
  CHECK(record.successful_boots == 23u && record.safe_mode == 0u);
  CHECK(record.config_signature == config_signature(&config) && persistent_is_valid(&record));
  CHECK(mock_system0_clock_hz() == config.clock_hz &&
    mock_system0_peripheral_mask() == config.peripheral_mask);
  CHECK(!mock_system0_invalid_access());
  return true;
}

static void corrupt_record(system_init_persistent_t *record, size_t index) {
  switch (index) {
    case 0: record->magic ^= 1u; break;
    case 1: record->config_signature ^= 1u; break;
    case 2: record->successful_boots ^= 1u; break;
    case 3: record->safe_mode = 2u; break;
    case 4: record->reserved = 1u; break;
    case 5: record->checksum ^= 1u; break;
    case 6: record->magic ^= 1u; record->checksum = persistent_checksum(record); break;
    case 7: record->safe_mode = 2u; record->checksum = persistent_checksum(record); break;
  }
}

static bool test_isolated_corruption_repair_and_rejection(void) {
  for (size_t index = 0; index < 8u; index++) {
    system_init_t state = { 0 };
    system_init_persistent_t record = retained_record(17u, true);
    corrupt_record(&record, index);
    mock_system0_reset();
    CHECK(initialize(&state, &record) == SYSTEM_INIT_RESULT_CONFIGURED);
    CHECK(record.successful_boots == 1u && record.safe_mode == 0u && record.reserved == 0u);
    CHECK(record.config_signature == config_signature(&valid_config));
    CHECK(persistent_is_valid(&record));
    CHECK(state.event == SYSTEM_INIT_EVENT_INITIALIZED && !state.safe_mode);
    CHECK(!mock_system0_invalid_access());

    state = (system_init_t) { 0 };
    record = retained_record(17u, true);
    mock_system0_reset();
    CHECK(initialize(&state, &record) == SYSTEM_INIT_RESULT_SAFE_MODE_LATCHED);
    CHECK(system_init_take_event(&state) == SYSTEM_INIT_EVENT_ENTERED_SAFE_MODE);
    corrupt_record(&record, index);
    const system_init_persistent_t before = record;
    const system_init_t state_before = state;
    mock_system0_set_irq_state(0xA5u);
    const size_t offset = mock_system0_event_count();
    CHECK(!system_init_resume(&state));
    CHECK(events_match_from(offset, (const expected_event_t[]) {
      { MOCK_SYSTEM_EVENT_IRQ_SAVE_DISABLE, 0xA5u },
      { MOCK_SYSTEM_EVENT_CONTROL_WRITE, SYSTEM0_CONTROL_SAFE },
      { MOCK_SYSTEM_EVENT_IRQ_RESTORE, 0xA5u },
    }, 3u));
    CHECK(state_equals(&state, &state_before) && persistent_equals(&record, &before));
    CHECK(mock_system0_control() == SYSTEM0_CONTROL_SAFE && !mock_system0_invalid_access());
  }
  return true;
}

static bool test_transition_rejections_and_empty_event(void) {
  system_init_t state = { 0 };
  system_init_persistent_t record = { 0 };
  mock_system0_reset();
  CHECK(initialize(&state, &record) == SYSTEM_INIT_RESULT_CONFIGURED);
  for (size_t stage = 0; stage < 5u; stage++) {
    const system_init_t before = state;
    const system_init_persistent_t record_before = record;
    size_t offset = mock_system0_event_count();
    mock_system0_set_irq_state(0x5Au);
    if (stage == 0u || stage == 2u || stage == 3u || stage == 4u) {
      CHECK(!system_init_enter_safe_mode(&state));
      CHECK(events_match_from(offset, (const expected_event_t[]) {
        { MOCK_SYSTEM_EVENT_IRQ_SAVE_DISABLE, 0x5Au },
        { MOCK_SYSTEM_EVENT_IRQ_RESTORE, 0x5Au },
      }, 2u));
      offset = mock_system0_event_count();
    }
    if (stage != 3u) {
      CHECK(!system_init_resume(&state));
      CHECK(events_match_from(offset, (const expected_event_t[]) {
        { MOCK_SYSTEM_EVENT_IRQ_SAVE_DISABLE, 0x5Au },
        { MOCK_SYSTEM_EVENT_IRQ_RESTORE, 0x5Au },
      }, 2u));
    }
    CHECK(state_equals(&state, &before) && persistent_equals(&record, &record_before));
    if (stage == 0u) CHECK(system_init_take_event(&state) == SYSTEM_INIT_EVENT_INITIALIZED);
    if (stage == 1u) CHECK(system_init_enter_safe_mode(&state));
    if (stage == 2u) CHECK(system_init_take_event(&state) == SYSTEM_INIT_EVENT_ENTERED_SAFE_MODE);
    if (stage == 3u) {
      /* Safe with no pending event: repeated entry rejects, resume now succeeds. */
      CHECK(system_init_resume(&state));
    }
    if (stage == 4u) CHECK(system_init_take_event(&state) == SYSTEM_INIT_EVENT_RESUMED);
  }
  const system_init_t before = state;
  const system_init_persistent_t record_before = record;
  const size_t offset = mock_system0_event_count();
  CHECK(system_init_take_event(&state) == SYSTEM_INIT_EVENT_NONE);
  CHECK(events_match_from(offset, (const expected_event_t[]) {
    { MOCK_SYSTEM_EVENT_IRQ_SAVE_DISABLE, 0x5Au },
    { MOCK_SYSTEM_EVENT_IRQ_RESTORE, 0x5Au },
  }, 2u));
  CHECK(state_equals(&state, &before) && persistent_equals(&record, &record_before));
  CHECK(!mock_system0_invalid_access());
  return true;
}

static bool test_usable_state_and_disabled_interrupts(void) {
  system_init_persistent_t record = { 0 };
  for (size_t index = 0; index < 3u; index++) {
    system_init_t state = {
      .system = NULL, .persistent = NULL, .config = valid_config,
      .event = SYSTEM_INIT_EVENT_RESUMED, .safe_mode = true, .initialized = true,
    };
    mock_system0_reset();
    if (index == 0u) state.system = mock_system0();
    if (index == 1u) state.persistent = &record;
    const system_init_t before = state;
    CHECK(!system_init_enter_safe_mode(&state) && !system_init_resume(&state));
    CHECK(system_init_take_event(&state) == SYSTEM_INIT_EVENT_NONE);
    CHECK(state_equals(&state, &before) && mock_system0_event_count() == 0u);
    CHECK(!mock_system0_invalid_access());
  }
  system_init_t state = { 0 };
  mock_system0_reset();
  mock_system0_set_irq_state(0u);
  CHECK(initialize(&state, &record) == SYSTEM_INIT_RESULT_CONFIGURED);
  CHECK(system_init_take_event(&state) == SYSTEM_INIT_EVENT_INITIALIZED);
  CHECK(system_init_enter_safe_mode(&state));
  CHECK(system_init_take_event(&state) == SYSTEM_INIT_EVENT_ENTERED_SAFE_MODE);
  CHECK(system_init_resume(&state));
  CHECK(system_init_take_event(&state) == SYSTEM_INIT_EVENT_RESUMED);
  CHECK(mock_system0_irq_state() == 0u);
  CHECK(!mock_system0_invalid_access());
  return true;
}

static bool test_reboot_count_and_safe_boot_preservation(void) {
  system_init_persistent_t record = retained_record(UINT16_MAX - 1u, false);
  for (size_t index = 0; index < 3u; index++) {
    system_init_t state = { 0 };
    mock_system0_reset();
    CHECK(initialize(&state, &record) == SYSTEM_INIT_RESULT_CONFIGURED);
    CHECK(record.successful_boots == UINT16_MAX && persistent_is_valid(&record));
    CHECK(system_init_take_event(&state) == SYSTEM_INIT_EVENT_INITIALIZED);
    CHECK(system_init_enter_safe_mode(&state));
    const system_init_persistent_t before = record;
    system_init_t reboot = { 0 };
    mock_system0_reset();
    CHECK(initialize(&reboot, &record) == SYSTEM_INIT_RESULT_SAFE_MODE_LATCHED);
    CHECK(persistent_equals(&record, &before));
    CHECK(system_init_take_event(&reboot) == SYSTEM_INIT_EVENT_ENTERED_SAFE_MODE);
    CHECK(system_init_resume(&reboot));
    CHECK(record.successful_boots == UINT16_MAX && persistent_is_valid(&record));
    CHECK(!mock_system0_invalid_access());
  }
  return true;
}


static bool test_updates_visible_before_interrupt_restore(void) {
  const uint32_t irq_values[] = { 0u, 1u, 0xA5u };
  for (size_t irq = 0; irq < 3u; irq++) {
    system_init_t state = { 0 };
    system_init_persistent_t record = retained_record(12u, false);
    mock_system0_reset();
    mock_system0_watch(&state, &record);
    mock_system0_set_irq_state(irq_values[irq]);
    CHECK(initialize(&state, &record) == SYSTEM_INIT_RESULT_CONFIGURED);
    for (size_t step = 0; step < 5u; step++) {
      const size_t offset = mock_system0_event_count();
      if (step == 1u) CHECK(system_init_enter_safe_mode(&state));
      else if (step == 3u) CHECK(system_init_resume(&state));
      else {
        const system_init_event_t expected = step == 0u ? SYSTEM_INIT_EVENT_INITIALIZED :
          step == 2u ? SYSTEM_INIT_EVENT_ENTERED_SAFE_MODE : SYSTEM_INIT_EVENT_RESUMED;
        CHECK(system_init_take_event(&state) == expected);
      }
      const size_t count = step == 1u ? 3u : step == 3u ? 6u : 2u;
      CHECK(mock_system0_event_count() == offset + count);
      CHECK(mock_system0_event_at(offset) == MOCK_SYSTEM_EVENT_IRQ_SAVE_DISABLE);
      CHECK(mock_system0_event_value(offset) == irq_values[irq]);
      const size_t last = offset + count - 1u;
      CHECK(mock_system0_event_at(last) == MOCK_SYSTEM_EVENT_IRQ_RESTORE);
      CHECK(mock_system0_event_value(last) == irq_values[irq]);
      CHECK(state_equals(mock_system0_state_at(last), &state));
      CHECK(persistent_equals(mock_system0_record_at(last), &record));
      CHECK(persistent_is_valid(mock_system0_record_at(last)));
      CHECK(record.successful_boots == 13u);
      CHECK(record.safe_mode == (state.safe_mode ? 1u : 0u));
      CHECK(state.event == (step == 1u ? SYSTEM_INIT_EVENT_ENTERED_SAFE_MODE :
        step == 3u ? SYSTEM_INIT_EVENT_RESUMED : SYSTEM_INIT_EVENT_NONE));
    }
    CHECK(mock_system0_irq_state() == irq_values[irq] && !mock_system0_invalid_access());
  }
  return true;
}

static bool test_aliasing_and_valid_mismatched_resume(void) {
  system_init_t state = { .config = { SYSTEM0_MIN_CLOCK_HZ, 15u } };
  system_init_persistent_t record = retained_record(31u, false);
  mock_system0_reset();
  CHECK(system_init_initialize(&state, mock_system0(), &record, &state.config) ==
    SYSTEM_INIT_RESULT_CONFIGURED);
  CHECK(state.config.clock_hz == SYSTEM0_MIN_CLOCK_HZ && state.config.peripheral_mask == 15u);
  CHECK(record.successful_boots == 32u && record.config_signature == config_signature(&state.config));
  CHECK(system_init_take_event(&state) == SYSTEM_INIT_EVENT_INITIALIZED);
  CHECK(system_init_enter_safe_mode(&state));
  CHECK(system_init_take_event(&state) == SYSTEM_INIT_EVENT_ENTERED_SAFE_MODE);
  /* Validity is structural; neither signature nor retained safe bit adds a gate. */
  record.config_signature ^= 1u;
  record.safe_mode = 0u;
  record.checksum = persistent_checksum(&record);
  CHECK(system_init_resume(&state));
  CHECK(record.successful_boots == 32u && record.config_signature == config_signature(&state.config));
  CHECK(persistent_is_valid(&record) && !state.safe_mode);
  CHECK(system_init_take_event(&state) == SYSTEM_INIT_EVENT_RESUMED);
  /* Enter repairs corrupt records while preserving their current counter. */
  record.reserved = 1u;
  record.checksum ^= 1u;
  CHECK(system_init_enter_safe_mode(&state));
  CHECK(record.successful_boots == 32u && record.safe_mode == 1u);
  CHECK(persistent_is_valid(&record) && !mock_system0_invalid_access());
  return true;
}


static bool test_retained_repair_precedes_controller_programming(void) {
  for (size_t corruption = 0; corruption < 8u; corruption++) {
    system_init_t state = { 0 };
    system_init_persistent_t record = retained_record(17u, true);
    corrupt_record(&record, corruption);
    const system_init_persistent_t repaired = retained_record(0u, false);
    mock_system0_reset();
    mock_system0_watch(&state, &record);
    CHECK(initialize(&state, &record) == SYSTEM_INIT_RESULT_CONFIGURED);
    CHECK(mock_system0_event_count() == 4u);
    for (size_t index = 0; index < 4u; index++) {
      CHECK(persistent_equals(mock_system0_record_at(index), &repaired));
    }
    CHECK(record.successful_boots == 1u && persistent_is_valid(&record));
    CHECK(!mock_system0_invalid_access());
  }
  system_init_t state = { 0 };
  system_init_persistent_t record = retained_record(19u, false);
  const system_init_persistent_t before = record;
  const system_init_config_t config = { SYSTEM0_MIN_CLOCK_HZ, 15u };
  mock_system0_reset();
  mock_system0_watch(&state, &record);
  CHECK(system_init_initialize(&state, mock_system0(), &record, &config) ==
    SYSTEM_INIT_RESULT_CONFIGURED);
  CHECK(mock_system0_event_count() == 4u);
  for (size_t index = 0; index < 4u; index++) {
    CHECK(persistent_equals(mock_system0_record_at(index), &before));
  }
  CHECK(record.successful_boots == 20u && persistent_is_valid(&record));
  CHECK(record.config_signature == config_signature(&config));
  CHECK(!mock_system0_invalid_access());
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "validation and idempotency", test_validation_initialization_and_idempotency },
    { "safe mode and resume", test_explicit_safe_mode_and_resume },
    { "retained safe boot", test_retained_safe_boot_integrity_and_saturation },
    { "invalid foreground calls", test_invalid_foreground_calls_have_no_side_effects },
    { "clock and mask boundaries", test_clock_and_mask_boundaries },
    { "repeat and conflict snapshots", test_repeat_and_conflict_preserve_every_field },
    { "retained safe boot with new config", test_retained_safe_boot_with_new_configuration },
    { "isolated retained corruption", test_isolated_corruption_repair_and_rejection },
    { "transition rejection and empty event", test_transition_rejections_and_empty_event },
    { "usable state and disabled IRQs", test_usable_state_and_disabled_interrupts },
    { "reboot count and safe preservation", test_reboot_count_and_safe_boot_preservation },
    { "updates before IRQ restore", test_updates_visible_before_interrupt_restore },
    { "config alias and valid mismatched resume", test_aliasing_and_valid_mismatched_resume },
    { "repair before controller programming", test_retained_repair_precedes_controller_programming },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) {
      fprintf(stderr, "failed: %s\n", tests[index].name);
      return 1;
    }
  }

  printf("Idempotent system-init public tests passed (%zu tests).\n",
    sizeof(tests) / sizeof(tests[0]));
  return 0;
}

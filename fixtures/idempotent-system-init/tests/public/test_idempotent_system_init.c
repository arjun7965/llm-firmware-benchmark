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
  CHECK(persistent.magic == before_persistent.magic);
  CHECK(persistent.checksum == before_persistent.checksum);
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

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "validation and idempotency", test_validation_initialization_and_idempotency },
    { "safe mode and resume", test_explicit_safe_mode_and_resume },
    { "retained safe boot", test_retained_safe_boot_integrity_and_saturation },
    { "invalid foreground calls", test_invalid_foreground_calls_have_no_side_effects },
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

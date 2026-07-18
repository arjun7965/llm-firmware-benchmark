#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "event_flags_deadlock.h"
#include "mock_rtos_events.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
              __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

static bool test_initialization_and_create_failure(void) {
  supervisor_coordination_t supervisor = { 0 };

  mock_rtos_events_reset();
  CHECK(!supervisor_coordination_init(NULL));
  CHECK(mock_rtos_events_create_count() == 0u);
  CHECK(mock_rtos_mutex_create_count() == 0u);

  CHECK(supervisor_coordination_init(&supervisor));
  CHECK(supervisor.initialized);
  CHECK(supervisor.events != NULL);
  CHECK(supervisor.configuration != NULL);
  CHECK(supervisor.actuator != NULL);
  CHECK(mock_rtos_events_create_count() == 1u);
  CHECK(mock_rtos_mutex_create_count() == 2u);
  CHECK(supervisor_coordination_init(&supervisor));
  CHECK(mock_rtos_events_create_count() == 1u);
  CHECK(mock_rtos_mutex_create_count() == 2u);

  supervisor = (supervisor_coordination_t) { 0 };
  mock_rtos_events_reset();
  mock_rtos_events_fail_next_event_create();
  CHECK(!supervisor_coordination_init(&supervisor));
  CHECK(!supervisor.initialized);
  CHECK(supervisor.events == NULL);
  CHECK(supervisor.configuration == NULL);
  CHECK(supervisor.actuator == NULL);
  CHECK(mock_rtos_events_create_count() == 1u);
  CHECK(mock_rtos_mutex_create_count() == 0u);

  supervisor = (supervisor_coordination_t) { 0 };
  mock_rtos_events_reset();
  mock_rtos_events_fail_next_mutex_create();
  CHECK(!supervisor_coordination_init(&supervisor));
  CHECK(!supervisor.initialized);
  CHECK(supervisor.events == NULL);
  CHECK(supervisor.configuration == NULL);
  CHECK(supervisor.actuator == NULL);
  CHECK(mock_rtos_events_create_count() == 1u);
  CHECK(mock_rtos_mutex_create_count() == 1u);
  return true;
}

static bool test_invalid_calls_have_no_rtos_side_effects(void) {
  supervisor_coordination_t supervisor = { 0 };
  uint32_t received = UINT32_C(0xa5a5a5a5);

  mock_rtos_events_reset();
  CHECK(supervisor_signal(NULL, SUPERVISOR_EVENT_FAULT) == RTOS_STATUS_INVALID_ARGUMENT);
  CHECK(supervisor_wait(NULL, &received) == RTOS_STATUS_INVALID_ARGUMENT);
  CHECK(supervisor_apply_configuration(NULL, 1u) == RTOS_STATUS_INVALID_ARGUMENT);
  CHECK(supervisor_signal(&supervisor, SUPERVISOR_EVENT_FAULT) == RTOS_STATUS_INVALID_ARGUMENT);
  CHECK(supervisor_wait(&supervisor, &received) == RTOS_STATUS_INVALID_ARGUMENT);
  CHECK(supervisor_apply_configuration(&supervisor, 1u) == RTOS_STATUS_INVALID_ARGUMENT);
  CHECK(received == UINT32_C(0xa5a5a5a5));
  CHECK(mock_rtos_event_set_count() == 0u);
  CHECK(mock_rtos_event_wait_count() == 0u);
  CHECK(mock_rtos_lock_count() == 0u);

  CHECK(supervisor_coordination_init(&supervisor));
  CHECK(supervisor_signal(&supervisor, 0u) == RTOS_STATUS_INVALID_ARGUMENT);
  CHECK(
    supervisor_signal(&supervisor, UINT32_C(0x80)) ==
      RTOS_STATUS_INVALID_ARGUMENT
  );
  supervisor.initialized = false;
  CHECK(supervisor_apply_configuration(&supervisor, 1u) == RTOS_STATUS_INVALID_ARGUMENT);
  CHECK(mock_rtos_event_set_count() == 0u);
  CHECK(mock_rtos_lock_count() == 0u);
  return true;
}

static bool test_event_flags_wait_any_clear_and_bound(void) {
  supervisor_coordination_t supervisor = { 0 };
  uint32_t received = 0u;
  const uint32_t expected = SUPERVISOR_EVENT_SAMPLE_READY |
    SUPERVISOR_EVENT_FAULT;

  mock_rtos_events_reset();
  CHECK(supervisor_coordination_init(&supervisor));
  CHECK(supervisor_signal(&supervisor, expected) == RTOS_STATUS_OK);
  CHECK(mock_rtos_event_set_count() == 1u);
  CHECK(mock_rtos_last_event_set_bits() == expected);
  CHECK(mock_rtos_event_bits() == expected);

  CHECK(supervisor_wait(&supervisor, &received) == RTOS_STATUS_OK);
  CHECK(received == expected);
  CHECK(mock_rtos_last_wait_bits() == SUPERVISOR_EVENT_MASK);
  CHECK(!mock_rtos_last_wait_all());
  CHECK(mock_rtos_last_clear_on_exit());
  CHECK(mock_rtos_last_wait_timeout() == SUPERVISOR_EVENT_WAIT_TICKS);
  CHECK(mock_rtos_event_bits() == 0u);

  received = UINT32_C(0x12345678);
  CHECK(supervisor_wait(&supervisor, &received) == RTOS_STATUS_TIMEOUT);
  CHECK(received == UINT32_C(0x12345678));
  return true;
}

static bool test_lock_order_apply_and_release(void) {
  supervisor_coordination_t supervisor = { 0 };

  mock_rtos_events_reset();
  CHECK(supervisor_coordination_init(&supervisor));
  CHECK(supervisor_apply_configuration(&supervisor, 17u) == RTOS_STATUS_OK);
  CHECK(mock_rtos_lock_count() == 2u);
  CHECK(
    mock_rtos_lock_timeout(0u) == CONFIGURATION_LOCK_TIMEOUT_TICKS
  );
  CHECK(
    mock_rtos_lock_timeout(1u) == CONFIGURATION_LOCK_TIMEOUT_TICKS
  );
  CHECK(mock_rtos_unlock_count() == 2u);
  CHECK(mock_rtos_apply_count() == 1u);
  CHECK(!mock_rtos_configuration_locked());
  CHECK(!mock_rtos_actuator_locked());
  CHECK(mock_rtos_operation_count() == 5u);
  CHECK(
    mock_rtos_operation(0u) == MOCK_RTOS_OPERATION_CONFIGURATION_LOCK
  );
  CHECK(mock_rtos_operation(1u) == MOCK_RTOS_OPERATION_ACTUATOR_LOCK);
  CHECK(
    mock_rtos_operation(2u) == MOCK_RTOS_OPERATION_CONFIGURATION_APPLY
  );
  CHECK(mock_rtos_operation(3u) == MOCK_RTOS_OPERATION_ACTUATOR_UNLOCK);
  CHECK(
    mock_rtos_operation(4u) == MOCK_RTOS_OPERATION_CONFIGURATION_UNLOCK
  );
  return true;
}

static bool test_contention_releases_first_lock_and_never_waits_forever(void) {
  supervisor_coordination_t supervisor = { 0 };

  mock_rtos_events_reset();
  CHECK(supervisor_coordination_init(&supervisor));
  mock_rtos_events_lock_actuator_from_peer();
  CHECK(
    supervisor_apply_configuration(&supervisor, 2u) == RTOS_STATUS_TIMEOUT
  );
  CHECK(mock_rtos_lock_count() == 2u);
  CHECK(mock_rtos_unlock_count() == 1u);
  CHECK(mock_rtos_apply_count() == 0u);
  CHECK(!mock_rtos_configuration_locked());
  CHECK(mock_rtos_actuator_locked());
  CHECK(mock_rtos_operation_count() == 3u);
  CHECK(
    mock_rtos_operation(0u) == MOCK_RTOS_OPERATION_CONFIGURATION_LOCK
  );
  CHECK(mock_rtos_operation(1u) == MOCK_RTOS_OPERATION_ACTUATOR_LOCK);
  CHECK(
    mock_rtos_operation(2u) == MOCK_RTOS_OPERATION_CONFIGURATION_UNLOCK
  );
  mock_rtos_events_unlock_actuator_from_peer();

  mock_rtos_events_force_next_apply_status(RTOS_STATUS_ERROR);
  CHECK(
    supervisor_apply_configuration(&supervisor, 3u) == RTOS_STATUS_ERROR
  );
  CHECK(!mock_rtos_configuration_locked());
  CHECK(!mock_rtos_actuator_locked());
  CHECK(mock_rtos_apply_count() == 1u);
  return true;
}

static bool test_unlock_error_precedence_still_attempts_both_releases(void) {
  supervisor_coordination_t supervisor = { 0 };

  mock_rtos_events_reset();
  CHECK(supervisor_coordination_init(&supervisor));
  mock_rtos_events_force_unlock_status_on_call(1u, RTOS_STATUS_ERROR);
  CHECK(
    supervisor_apply_configuration(&supervisor, 4u) == RTOS_STATUS_ERROR
  );
  CHECK(mock_rtos_apply_count() == 1u);
  CHECK(mock_rtos_unlock_count() == 2u);
  CHECK(!mock_rtos_configuration_locked());
  CHECK(mock_rtos_actuator_locked());

  mock_rtos_events_reset();
  supervisor = (supervisor_coordination_t) { 0 };
  CHECK(supervisor_coordination_init(&supervisor));
  mock_rtos_events_force_unlock_status_on_call(2u, RTOS_STATUS_ERROR);
  CHECK(
    supervisor_apply_configuration(&supervisor, 5u) == RTOS_STATUS_ERROR
  );
  CHECK(mock_rtos_apply_count() == 1u);
  CHECK(mock_rtos_unlock_count() == 2u);
  CHECK(!mock_rtos_actuator_locked());
  CHECK(mock_rtos_configuration_locked());
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "initialization and creation failure", test_initialization_and_create_failure },
    { "invalid calls", test_invalid_calls_have_no_rtos_side_effects },
    { "event wait and clear", test_event_flags_wait_any_clear_and_bound },
    { "lock order", test_lock_order_apply_and_release },
    { "contention rollback", test_contention_releases_first_lock_and_never_waits_forever },
    { "unlock error precedence", test_unlock_error_precedence_still_attempts_both_releases },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) return 1;
    printf("ok - %s\n", tests[index].name);
  }
  return 0;
}

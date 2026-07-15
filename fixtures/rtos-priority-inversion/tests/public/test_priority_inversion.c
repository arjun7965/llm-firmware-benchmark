#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "mock_rtos.h"
#include "priority_inversion.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
              __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

static bool register_scenario_tasks(void) {
  return mock_rtos_register_task(MOCK_RTOS_TASK_TELEMETRY, 1u) &&
    mock_rtos_register_task(MOCK_RTOS_TASK_DIAGNOSTICS, 2u) &&
    mock_rtos_register_task(MOCK_RTOS_TASK_SAFETY, 3u);
}

static bool test_initialization_and_create_failure(void) {
  priority_guard_t guard = { 0 };

  mock_rtos_reset();
  CHECK(!priority_guard_init(NULL));
  CHECK(mock_rtos_mutex_create_count() == 0u);

  CHECK(priority_guard_init(&guard));
  CHECK(guard.mutex != NULL);
  CHECK(guard.initialized);
  CHECK(mock_rtos_mutex_create_count() == 1u);
  CHECK(mock_rtos_last_mutex_has_priority_inheritance());
  CHECK(priority_guard_init(&guard));
  CHECK(mock_rtos_mutex_create_count() == 1u);

  guard.initialized = false;
  mock_rtos_reset();
  mock_rtos_fail_next_mutex_create();
  CHECK(!priority_guard_init(&guard));
  CHECK(guard.mutex == NULL);
  CHECK(!guard.initialized);
  CHECK(mock_rtos_mutex_create_count() == 1u);
  return true;
}

static bool test_uninitialized_guard_makes_no_rtos_calls(void) {
  priority_guard_t guard = { 0 };

  mock_rtos_reset();
  CHECK(
    priority_guard_lock_telemetry(NULL) == RTOS_STATUS_INVALID_ARGUMENT
  );
  CHECK(priority_guard_lock_safety(NULL) == RTOS_STATUS_INVALID_ARGUMENT);
  CHECK(priority_guard_unlock(NULL) == RTOS_STATUS_INVALID_ARGUMENT);
  CHECK(
    priority_guard_lock_telemetry(&guard) == RTOS_STATUS_INVALID_ARGUMENT
  );
  CHECK(priority_guard_unlock(&guard) == RTOS_STATUS_INVALID_ARGUMENT);
  guard.initialized = true;
  CHECK(
    priority_guard_lock_safety(&guard) == RTOS_STATUS_INVALID_ARGUMENT
  );
  CHECK(priority_guard_unlock(&guard) == RTOS_STATUS_INVALID_ARGUMENT);
  CHECK(mock_rtos_lock_count() == 0u);
  CHECK(mock_rtos_unlock_count() == 0u);

  guard.initialized = false;
  CHECK(priority_guard_init(&guard));
  CHECK(register_scenario_tasks());
  mock_rtos_set_current_task(MOCK_RTOS_TASK_SAFETY);
  guard.initialized = false;
  CHECK(
    priority_guard_lock_safety(&guard) == RTOS_STATUS_INVALID_ARGUMENT
  );
  CHECK(mock_rtos_lock_count() == 0u);
  return true;
}

static bool test_lock_wait_policies_and_error_propagation(void) {
  priority_guard_t guard = { 0 };

  mock_rtos_reset();
  CHECK(priority_guard_init(&guard));
  CHECK(register_scenario_tasks());

  mock_rtos_set_current_task(MOCK_RTOS_TASK_TELEMETRY);
  CHECK(priority_guard_lock_telemetry(&guard) == RTOS_STATUS_OK);
  CHECK(mock_rtos_lock_count() == 1u);
  CHECK(mock_rtos_last_lock_timeout_ticks() == RTOS_WAIT_FOREVER);
  CHECK(priority_guard_unlock(&guard) == RTOS_STATUS_OK);

  mock_rtos_set_current_task(MOCK_RTOS_TASK_SAFETY);
  CHECK(priority_guard_lock_safety(&guard) == RTOS_STATUS_OK);
  CHECK(mock_rtos_lock_count() == 2u);
  CHECK(
    mock_rtos_last_lock_timeout_ticks() == SAFETY_LOCK_TIMEOUT_TICKS
  );
  CHECK(priority_guard_unlock(&guard) == RTOS_STATUS_OK);

  mock_rtos_force_next_lock_status(RTOS_STATUS_TIMEOUT);
  CHECK(priority_guard_lock_safety(&guard) == RTOS_STATUS_TIMEOUT);
  CHECK(mock_rtos_lock_count() == 3u);
  CHECK(mock_rtos_unlock_count() == 2u);
  CHECK(priority_guard_unlock(&guard) == RTOS_STATUS_NOT_OWNER);
  CHECK(mock_rtos_unlock_count() == 3u);
  return true;
}

static bool test_priority_inheritance_prevents_medium_preemption(void) {
  priority_guard_t guard = { 0 };

  mock_rtos_reset();
  CHECK(priority_guard_init(&guard));
  CHECK(register_scenario_tasks());

  mock_rtos_set_current_task(MOCK_RTOS_TASK_TELEMETRY);
  CHECK(priority_guard_lock_telemetry(&guard) == RTOS_STATUS_OK);

  mock_rtos_set_current_task(MOCK_RTOS_TASK_SAFETY);
  CHECK(priority_guard_lock_safety(&guard) == RTOS_STATUS_BLOCKED);
  CHECK(mock_rtos_task_is_blocked(MOCK_RTOS_TASK_SAFETY));
  CHECK(mock_rtos_effective_priority(MOCK_RTOS_TASK_TELEMETRY) == 3u);
  CHECK(mock_rtos_next_runnable_task() == MOCK_RTOS_TASK_TELEMETRY);

  mock_rtos_set_current_task(MOCK_RTOS_TASK_TELEMETRY);
  CHECK(priority_guard_unlock(&guard) == RTOS_STATUS_OK);
  CHECK(mock_rtos_effective_priority(MOCK_RTOS_TASK_TELEMETRY) == 1u);
  CHECK(!mock_rtos_task_is_blocked(MOCK_RTOS_TASK_SAFETY));
  CHECK(mock_rtos_next_runnable_task() == MOCK_RTOS_TASK_SAFETY);

  mock_rtos_set_current_task(MOCK_RTOS_TASK_SAFETY);
  CHECK(priority_guard_lock_safety(&guard) == RTOS_STATUS_OK);
  CHECK(priority_guard_unlock(&guard) == RTOS_STATUS_OK);
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    {
      "initialization and create failure",
      test_initialization_and_create_failure,
    },
    {
      "uninitialized guard",
      test_uninitialized_guard_makes_no_rtos_calls,
    },
    {
      "wait policies and errors",
      test_lock_wait_policies_and_error_propagation,
    },
    {
      "priority inheritance",
      test_priority_inheritance_prevents_medium_preemption,
    },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) return 1;
    printf("ok - %s\n", tests[index].name);
  }
  return 0;
}

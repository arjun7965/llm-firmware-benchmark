#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "mock_real_time_guard.h"
#include "real_time_guard.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
        __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

typedef struct {
  mock_realtime_event_t event;
  real_time_task_t task;
  real_time_violation_t violation;
  uint32_t release;
  uint32_t time;
} expected_event_t;

static bool state_equals(
  const real_time_guard_t *left,
  const real_time_guard_t *right
) {
  return left->next_control_release == right->next_control_release &&
    left->next_telemetry_release == right->next_telemetry_release &&
    left->active_release == right->active_release &&
    left->active_started_at == right->active_started_at &&
    left->active_deadline == right->active_deadline &&
    left->active_task == right->active_task &&
    left->active == right->active &&
    left->initialized == right->initialized;
}

static bool events_match_from(
  size_t offset,
  const expected_event_t *expected,
  size_t expected_count
) {
  if (mock_realtime_event_count() != offset + expected_count) return false;
  for (size_t index = 0u; index < expected_count; index++) {
    if (mock_realtime_event_at(offset + index) != expected[index].event ||
        mock_realtime_event_task(offset + index) != expected[index].task ||
        mock_realtime_event_violation(offset + index) != expected[index].violation ||
        mock_realtime_event_release(offset + index) != expected[index].release ||
        mock_realtime_event_time(offset + index) != expected[index].time) {
      return false;
    }
  }
  return true;
}

static bool test_initialization_and_invalid_calls(void) {
  real_time_guard_t guard = {
    .next_control_release = 1u,
    .next_telemetry_release = 2u,
    .active_release = 3u,
    .active_started_at = 4u,
    .active_deadline = 5u,
    .active_task = REALTIME_TASK_CONTROL,
    .active = true,
    .initialized = true,
  };
  const real_time_guard_t before = guard;
  real_time_guard_t uninitialized = { 0 };

  mock_realtime_reset();
  CHECK(!real_time_guard_init(NULL, 100u));
  CHECK(state_equals(&guard, &before));
  CHECK(real_time_guard_dispatch(NULL, 0u) == REALTIME_STATUS_INVALID_ARGUMENT);
  CHECK(real_time_guard_complete(NULL, 0u) == REALTIME_STATUS_INVALID_ARGUMENT);
  CHECK(real_time_guard_dispatch(&uninitialized, 0u) ==
    REALTIME_STATUS_INVALID_ARGUMENT);
  CHECK(real_time_guard_complete(&uninitialized, 0u) ==
    REALTIME_STATUS_INVALID_ARGUMENT);
  CHECK(mock_realtime_event_count() == 0u);

  CHECK(real_time_guard_init(&guard, 100u));
  CHECK(guard.next_control_release == 110u);
  CHECK(guard.next_telemetry_release == 125u);
  CHECK(!guard.active);
  CHECK(guard.active_task == REALTIME_TASK_COUNT);
  CHECK(guard.initialized);
  CHECK(mock_realtime_event_count() == 0u);
  return true;
}

static bool test_control_release_budget_boundary_and_jitter(void) {
  real_time_guard_t guard = { 0 };
  size_t offset;

  mock_realtime_reset();
  CHECK(real_time_guard_init(&guard, 0u));
  CHECK(real_time_guard_dispatch(&guard, 9u) == REALTIME_STATUS_IDLE);
  CHECK(mock_realtime_event_count() == 0u);

  CHECK(real_time_guard_dispatch(&guard, 10u) == REALTIME_STATUS_OK);
  CHECK(events_match_from(
    0u,
    (const expected_event_t[]) {
      { MOCK_REALTIME_EVENT_BEGIN, REALTIME_TASK_CONTROL,
        REALTIME_VIOLATION_COUNT, 10u, 16u },
    },
    1u
  ));
  CHECK(guard.active);
  CHECK(guard.active_release == 10u);
  CHECK(guard.active_started_at == 10u);
  CHECK(guard.active_deadline == 16u);
  CHECK(guard.next_control_release == 20u);
  CHECK(real_time_guard_dispatch(&guard, 11u) == REALTIME_STATUS_BUSY);

  offset = mock_realtime_event_count();
  CHECK(real_time_guard_complete(&guard, 15u) == REALTIME_STATUS_OK);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_REALTIME_EVENT_FINISH, REALTIME_TASK_CONTROL,
        REALTIME_VIOLATION_COUNT, 0u, 0u },
    },
    1u
  ));
  CHECK(!guard.active);

  offset = mock_realtime_event_count();
  CHECK(real_time_guard_dispatch(&guard, 23u) ==
    REALTIME_STATUS_JITTER_VIOLATION);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_REALTIME_EVENT_VIOLATION, REALTIME_TASK_CONTROL,
        REALTIME_VIOLATION_JITTER, 20u, 23u },
    },
    1u
  ));
  CHECK(!guard.active);
  CHECK(guard.next_control_release == 33u);
  return true;
}

static bool test_deadline_uses_nominal_release_and_control_precedes_telemetry(void) {
  real_time_guard_t guard = { 0 };
  size_t offset;

  mock_realtime_reset();
  CHECK(real_time_guard_init(&guard, 0u));
  CHECK(real_time_guard_dispatch(&guard, 12u) == REALTIME_STATUS_OK);
  CHECK(events_match_from(
    0u,
    (const expected_event_t[]) {
      { MOCK_REALTIME_EVENT_BEGIN, REALTIME_TASK_CONTROL,
        REALTIME_VIOLATION_COUNT, 10u, 16u },
    },
    1u
  ));
  CHECK(guard.active_deadline == 16u);
  CHECK(real_time_guard_complete(&guard, 16u) == REALTIME_STATUS_OK);
  CHECK(!guard.active);

  mock_realtime_reset();
  CHECK(real_time_guard_init(&guard, 0u));
  CHECK(real_time_guard_dispatch(&guard, 12u) == REALTIME_STATUS_OK);
  CHECK(guard.active_deadline == 16u);
  offset = mock_realtime_event_count();
  CHECK(real_time_guard_complete(&guard, 17u) ==
    REALTIME_STATUS_DEADLINE_VIOLATION);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_REALTIME_EVENT_VIOLATION, REALTIME_TASK_CONTROL,
        REALTIME_VIOLATION_DEADLINE, 10u, 17u },
    },
    1u
  ));
  CHECK(!guard.active);

  offset = mock_realtime_event_count();
  CHECK(real_time_guard_dispatch(&guard, 25u) ==
    REALTIME_STATUS_JITTER_VIOLATION);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_REALTIME_EVENT_VIOLATION, REALTIME_TASK_CONTROL,
        REALTIME_VIOLATION_JITTER, 22u, 25u },
    },
    1u
  ));
  CHECK(guard.next_control_release == 35u);
  CHECK(guard.next_telemetry_release == 25u);

  offset = mock_realtime_event_count();
  CHECK(real_time_guard_dispatch(&guard, 25u) == REALTIME_STATUS_OK);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_REALTIME_EVENT_BEGIN, REALTIME_TASK_TELEMETRY,
        REALTIME_VIOLATION_COUNT, 25u, 37u },
    },
    1u
  ));
  CHECK(guard.active_task == REALTIME_TASK_TELEMETRY);
  return true;
}

static bool test_rtos_failure_retry_and_execution_budget_violation(void) {
  real_time_guard_t guard = { 0 };
  size_t offset;

  mock_realtime_reset();
  CHECK(real_time_guard_init(&guard, 0u));
  mock_realtime_fail_next_begin(RTOS_STATUS_ERROR);
  CHECK(real_time_guard_dispatch(&guard, 10u) == REALTIME_STATUS_RTOS_ERROR);
  CHECK(!guard.active);
  CHECK(guard.next_control_release == 10u);
  CHECK(mock_realtime_event_count() == 1u);

  CHECK(real_time_guard_dispatch(&guard, 10u) == REALTIME_STATUS_OK);
  CHECK(guard.active);
  mock_realtime_fail_next_finish(RTOS_STATUS_ERROR);
  CHECK(real_time_guard_complete(&guard, 12u) == REALTIME_STATUS_RTOS_ERROR);
  CHECK(guard.active);
  CHECK(real_time_guard_dispatch(&guard, 14u) == REALTIME_STATUS_BUSY);
  CHECK(real_time_guard_complete(&guard, 15u) == REALTIME_STATUS_OK);
  CHECK(!guard.active);

  CHECK(real_time_guard_dispatch(&guard, 20u) == REALTIME_STATUS_OK);
  offset = mock_realtime_event_count();
  CHECK(real_time_guard_complete(&guard, 26u) ==
    REALTIME_STATUS_BUDGET_VIOLATION);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_REALTIME_EVENT_VIOLATION, REALTIME_TASK_CONTROL,
        REALTIME_VIOLATION_BUDGET, 20u, 26u },
    },
    1u
  ));
  CHECK(!guard.active);
  return true;
}

static bool test_wraparound_and_telemetry_jitter_boundary(void) {
  real_time_guard_t guard = { 0 };

  mock_realtime_reset();
  CHECK(real_time_guard_init(&guard, UINT32_MAX - 12u));
  CHECK(guard.next_control_release == UINT32_MAX - 2u);
  CHECK(real_time_guard_dispatch(&guard, 1u) ==
    REALTIME_STATUS_JITTER_VIOLATION);
  CHECK(events_match_from(
    0u,
    (const expected_event_t[]) {
      { MOCK_REALTIME_EVENT_VIOLATION, REALTIME_TASK_CONTROL,
        REALTIME_VIOLATION_JITTER, UINT32_MAX - 2u, 1u },
    },
    1u
  ));

  mock_realtime_reset();
  CHECK(real_time_guard_init(&guard, UINT32_MAX - 5u));
  CHECK(guard.next_control_release == 4u);
  CHECK(guard.next_telemetry_release == 19u);
  CHECK(real_time_guard_dispatch(&guard, 3u) == REALTIME_STATUS_IDLE);
  CHECK(real_time_guard_dispatch(&guard, 4u) == REALTIME_STATUS_OK);
  CHECK(guard.active_deadline == 10u);
  CHECK(real_time_guard_complete(&guard, 9u) == REALTIME_STATUS_OK);

  mock_realtime_reset();
  CHECK(real_time_guard_init(&guard, 0u));
  CHECK(real_time_guard_dispatch(&guard, 10u) == REALTIME_STATUS_OK);
  CHECK(real_time_guard_complete(&guard, 15u) == REALTIME_STATUS_OK);
  CHECK(real_time_guard_dispatch(&guard, 20u) == REALTIME_STATUS_OK);
  CHECK(real_time_guard_complete(&guard, 25u) == REALTIME_STATUS_OK);
  CHECK(real_time_guard_dispatch(&guard, 29u) == REALTIME_STATUS_OK);
  CHECK(guard.active_task == REALTIME_TASK_TELEMETRY);
  CHECK(guard.active_release == 25u);
  CHECK(guard.active_deadline == 37u);
  CHECK(real_time_guard_complete(&guard, 35u) == REALTIME_STATUS_OK);
  CHECK(!guard.active);
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "initialization and invalid calls", test_initialization_and_invalid_calls },
    { "control budget and jitter", test_control_release_budget_boundary_and_jitter },
    { "deadline and priority", test_deadline_uses_nominal_release_and_control_precedes_telemetry },
    { "RTOS retry and budget violation", test_rtos_failure_retry_and_execution_budget_violation },
    { "wraparound and telemetry jitter", test_wraparound_and_telemetry_jitter_boundary },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) return 1;
    printf("ok - %s\n", tests[index].name);
  }
  return 0;
}

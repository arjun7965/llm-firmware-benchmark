#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "mock_rtos_scheduler.h"
#include "periodic_scheduler.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
              __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

static bool test_initialization_and_invalid_dispatch(void) {
  periodic_scheduler_t scheduler = { 0 };

  mock_rtos_scheduler_reset();
  CHECK(!periodic_scheduler_init(NULL, 10u));
  CHECK(mock_rtos_scheduler_release_count() == 0u);
  CHECK(
    periodic_scheduler_dispatch(&scheduler, 10u) ==
      RTOS_STATUS_INVALID_ARGUMENT
  );
  CHECK(mock_rtos_scheduler_release_count() == 0u);

  CHECK(periodic_scheduler_init(&scheduler, 100u));
  CHECK(scheduler.initialized);
  CHECK(scheduler.next_control_release == 105u);
  CHECK(scheduler.next_telemetry_release == 120u);
  CHECK(periodic_scheduler_dispatch(NULL, 100u) == RTOS_STATUS_INVALID_ARGUMENT);
  CHECK(mock_rtos_scheduler_release_count() == 0u);
  return true;
}

static bool test_control_precedes_telemetry_and_uses_bounded_deadlines(void) {
  periodic_scheduler_t scheduler = { 0 };

  mock_rtos_scheduler_reset();
  CHECK(periodic_scheduler_init(&scheduler, 100u));
  CHECK(periodic_scheduler_dispatch(&scheduler, 104u) == RTOS_STATUS_OK);
  CHECK(mock_rtos_scheduler_release_count() == 0u);

  CHECK(periodic_scheduler_dispatch(&scheduler, 120u) == RTOS_STATUS_OK);
  CHECK(mock_rtos_scheduler_release_count() == 2u);
  CHECK(
    mock_rtos_scheduler_release_task(0u) == RTOS_SCHEDULED_TASK_CONTROL
  );
  CHECK(mock_rtos_scheduler_release_deadline(0u) == 122u);
  CHECK(
    mock_rtos_scheduler_release_task(1u) == RTOS_SCHEDULED_TASK_TELEMETRY
  );
  CHECK(mock_rtos_scheduler_release_deadline(1u) == 130u);
  CHECK(scheduler.next_control_release == 125u);
  CHECK(scheduler.next_telemetry_release == 140u);
  return true;
}

static bool test_late_dispatch_collapses_missed_periods(void) {
  periodic_scheduler_t scheduler = { 0 };

  mock_rtos_scheduler_reset();
  CHECK(periodic_scheduler_init(&scheduler, 0u));
  CHECK(periodic_scheduler_dispatch(&scheduler, 100u) == RTOS_STATUS_OK);
  CHECK(mock_rtos_scheduler_release_count() == 2u);
  CHECK(scheduler.next_control_release == 105u);
  CHECK(scheduler.next_telemetry_release == 120u);
  CHECK(periodic_scheduler_dispatch(&scheduler, 100u) == RTOS_STATUS_OK);
  CHECK(mock_rtos_scheduler_release_count() == 2u);
  return true;
}

static bool test_release_failure_keeps_task_due_for_retry(void) {
  periodic_scheduler_t scheduler = { 0 };

  mock_rtos_scheduler_reset();
  CHECK(periodic_scheduler_init(&scheduler, 0u));
  mock_rtos_scheduler_fail_next_release(RTOS_STATUS_ERROR);
  CHECK(periodic_scheduler_dispatch(&scheduler, 5u) == RTOS_STATUS_ERROR);
  CHECK(mock_rtos_scheduler_release_count() == 1u);
  CHECK(scheduler.next_control_release == 5u);
  CHECK(scheduler.next_telemetry_release == 20u);

  CHECK(periodic_scheduler_dispatch(&scheduler, 6u) == RTOS_STATUS_OK);
  CHECK(mock_rtos_scheduler_release_count() == 2u);
  CHECK(
    mock_rtos_scheduler_release_task(1u) == RTOS_SCHEDULED_TASK_CONTROL
  );
  CHECK(mock_rtos_scheduler_release_deadline(1u) == 8u);
  CHECK(scheduler.next_control_release == 11u);
  return true;
}

static bool test_reinitialization_and_telemetry_failure(void) {
  periodic_scheduler_t scheduler = { 0 };

  mock_rtos_scheduler_reset();
  CHECK(periodic_scheduler_init(&scheduler, 10u));
  CHECK(periodic_scheduler_dispatch(&scheduler, 15u) == RTOS_STATUS_OK);
  CHECK(mock_rtos_scheduler_release_count() == 1u);
  CHECK(periodic_scheduler_init(&scheduler, 40u));
  CHECK(scheduler.next_control_release == 45u);
  CHECK(scheduler.next_telemetry_release == 60u);
  CHECK(mock_rtos_scheduler_release_count() == 1u);

  scheduler = (periodic_scheduler_t) { 0 };
  mock_rtos_scheduler_reset();
  CHECK(periodic_scheduler_init(&scheduler, 0u));
  CHECK(periodic_scheduler_dispatch(&scheduler, 5u) == RTOS_STATUS_OK);
  CHECK(periodic_scheduler_dispatch(&scheduler, 10u) == RTOS_STATUS_OK);
  CHECK(periodic_scheduler_dispatch(&scheduler, 15u) == RTOS_STATUS_OK);
  mock_rtos_scheduler_fail_release_on_call(5u, RTOS_STATUS_ERROR);
  CHECK(periodic_scheduler_dispatch(&scheduler, 20u) == RTOS_STATUS_ERROR);
  CHECK(mock_rtos_scheduler_release_count() == 5u);
  CHECK(scheduler.next_control_release == 25u);
  CHECK(scheduler.next_telemetry_release == 20u);
  CHECK(periodic_scheduler_dispatch(&scheduler, 21u) == RTOS_STATUS_OK);
  CHECK(mock_rtos_scheduler_release_count() == 6u);
  CHECK(
    mock_rtos_scheduler_release_task(5u) == RTOS_SCHEDULED_TASK_TELEMETRY
  );
  CHECK(mock_rtos_scheduler_release_deadline(5u) == 31u);
  CHECK(scheduler.next_control_release == 25u);
  CHECK(scheduler.next_telemetry_release == 41u);
  return true;
}

static bool test_tick_wraparound_is_explicit(void) {
  periodic_scheduler_t scheduler = { 0 };

  mock_rtos_scheduler_reset();
  CHECK(periodic_scheduler_init(&scheduler, UINT32_MAX - 2u));
  CHECK(scheduler.next_control_release == 2u);
  CHECK(periodic_scheduler_dispatch(&scheduler, 1u) == RTOS_STATUS_OK);
  CHECK(mock_rtos_scheduler_release_count() == 0u);
  CHECK(periodic_scheduler_dispatch(&scheduler, 2u) == RTOS_STATUS_OK);
  CHECK(mock_rtos_scheduler_release_count() == 1u);
  CHECK(mock_rtos_scheduler_release_deadline(0u) == 4u);
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "initialization and invalid dispatch", test_initialization_and_invalid_dispatch },
    { "priority order and deadlines", test_control_precedes_telemetry_and_uses_bounded_deadlines },
    { "late dispatch collapse", test_late_dispatch_collapses_missed_periods },
    { "release retry", test_release_failure_keeps_task_due_for_retry },
    { "reinitialization and telemetry retry", test_reinitialization_and_telemetry_failure },
    { "tick wraparound", test_tick_wraparound_is_explicit },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) return 1;
    printf("ok - %s\n", tests[index].name);
  }
  return 0;
}

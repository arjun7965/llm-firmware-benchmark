#include "mock_rtos_scheduler.h"

#include <stdbool.h>

#define MOCK_RTOS_SCHEDULER_MAX_RELEASES 16u

typedef struct {
  rtos_scheduled_task_t task;
  uint32_t deadline;
} release_record_t;

static release_record_t releases[MOCK_RTOS_SCHEDULER_MAX_RELEASES];
static size_t release_count;
static bool fail_next_release;
static size_t forced_release_call;
static rtos_status_t next_release_status;

void mock_rtos_scheduler_reset(void) {
  release_count = 0u;
  fail_next_release = false;
  forced_release_call = 0u;
  next_release_status = RTOS_STATUS_OK;
}

void mock_rtos_scheduler_fail_next_release(rtos_status_t status) {
  fail_next_release = true;
  forced_release_call = release_count + 1u;
  next_release_status = status;
}

void mock_rtos_scheduler_fail_release_on_call(
  size_t call_index,
  rtos_status_t status
) {
  fail_next_release = true;
  forced_release_call = call_index;
  next_release_status = status;
}

size_t mock_rtos_scheduler_release_count(void) {
  return release_count;
}

rtos_scheduled_task_t mock_rtos_scheduler_release_task(size_t index) {
  return index < release_count
    ? releases[index].task
    : RTOS_SCHEDULED_TASK_COUNT;
}

uint32_t mock_rtos_scheduler_release_deadline(size_t index) {
  return index < release_count ? releases[index].deadline : 0u;
}

rtos_status_t rtos_schedule_release(
  rtos_scheduled_task_t task,
  uint32_t absolute_deadline_ticks
) {
  if (
    task >= RTOS_SCHEDULED_TASK_COUNT ||
    release_count >= MOCK_RTOS_SCHEDULER_MAX_RELEASES
  ) {
    return RTOS_STATUS_INVALID_ARGUMENT;
  }

  const size_t call_index = release_count + 1u;
  releases[release_count++] = (release_record_t) {
    .task = task,
    .deadline = absolute_deadline_ticks,
  };
  if (!fail_next_release || call_index != forced_release_call) {
    return RTOS_STATUS_OK;
  }

  fail_next_release = false;
  return next_release_status;
}

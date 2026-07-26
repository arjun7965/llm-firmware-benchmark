#include "mock_real_time_guard.h"

#include <stdbool.h>

#define MOCK_REALTIME_HISTORY_CAPACITY 64u

typedef struct {
  mock_realtime_event_t event;
  real_time_task_t task;
  real_time_violation_t violation;
  uint32_t release;
  uint32_t time;
} mock_realtime_record_t;

typedef struct {
  mock_realtime_record_t records[MOCK_REALTIME_HISTORY_CAPACITY];
  size_t count;
  bool fail_begin;
  bool fail_finish;
  rtos_status_t begin_status;
  rtos_status_t finish_status;
} mock_realtime_state_t;

static mock_realtime_state_t state;

static void record(
  mock_realtime_event_t event,
  real_time_task_t task,
  real_time_violation_t violation,
  uint32_t release,
  uint32_t time
) {
  if (state.count < MOCK_REALTIME_HISTORY_CAPACITY) {
    state.records[state.count] = (mock_realtime_record_t) {
      .event = event,
      .task = task,
      .violation = violation,
      .release = release,
      .time = time,
    };
  }
  state.count++;
}

void mock_realtime_reset(void) {
  state = (mock_realtime_state_t) {
    .begin_status = RTOS_STATUS_OK,
    .finish_status = RTOS_STATUS_OK,
  };
}

void mock_realtime_fail_next_begin(rtos_status_t status) {
  state.fail_begin = true;
  state.begin_status = status;
}

void mock_realtime_fail_next_finish(rtos_status_t status) {
  state.fail_finish = true;
  state.finish_status = status;
}

size_t mock_realtime_event_count(void) {
  return state.count;
}

mock_realtime_event_t mock_realtime_event_at(size_t index) {
  return index < state.count && index < MOCK_REALTIME_HISTORY_CAPACITY
    ? state.records[index].event
    : MOCK_REALTIME_EVENT_COUNT;
}

real_time_task_t mock_realtime_event_task(size_t index) {
  return index < state.count && index < MOCK_REALTIME_HISTORY_CAPACITY
    ? state.records[index].task
    : REALTIME_TASK_COUNT;
}

real_time_violation_t mock_realtime_event_violation(size_t index) {
  return index < state.count && index < MOCK_REALTIME_HISTORY_CAPACITY
    ? state.records[index].violation
    : REALTIME_VIOLATION_COUNT;
}

uint32_t mock_realtime_event_release(size_t index) {
  return index < state.count && index < MOCK_REALTIME_HISTORY_CAPACITY
    ? state.records[index].release
    : 0u;
}

uint32_t mock_realtime_event_time(size_t index) {
  return index < state.count && index < MOCK_REALTIME_HISTORY_CAPACITY
    ? state.records[index].time
    : 0u;
}

rtos_status_t realtime_rtos_begin(
  real_time_task_t task,
  uint32_t nominal_release_ticks,
  uint32_t absolute_deadline_ticks
) {
  const rtos_status_t status = state.fail_begin
    ? state.begin_status
    : RTOS_STATUS_OK;

  record(
    MOCK_REALTIME_EVENT_BEGIN,
    task,
    REALTIME_VIOLATION_COUNT,
    nominal_release_ticks,
    absolute_deadline_ticks
  );
  state.fail_begin = false;
  return status;
}

rtos_status_t realtime_rtos_finish(real_time_task_t task) {
  const rtos_status_t status = state.fail_finish
    ? state.finish_status
    : RTOS_STATUS_OK;

  record(
    MOCK_REALTIME_EVENT_FINISH,
    task,
    REALTIME_VIOLATION_COUNT,
    0u,
    0u
  );
  state.fail_finish = false;
  return status;
}

void realtime_rtos_report_violation(
  real_time_task_t task,
  real_time_violation_t violation,
  uint32_t nominal_release_ticks,
  uint32_t observed_ticks
) {
  record(
    MOCK_REALTIME_EVENT_VIOLATION,
    task,
    violation,
    nominal_release_ticks,
    observed_ticks
  );
}

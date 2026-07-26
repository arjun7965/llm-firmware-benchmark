#include <stddef.h>

#include "real_time_guard.h"

static bool tick_has_reached(uint32_t now_ticks, uint32_t target_ticks) {
  return (uint32_t)(now_ticks - target_ticks) < UINT32_C(0x80000000);
}

static bool tick_is_after(uint32_t now_ticks, uint32_t target_ticks) {
  return now_ticks != target_ticks && tick_has_reached(now_ticks, target_ticks);
}

static uint32_t period_for(real_time_task_t task) {
  return task == REALTIME_TASK_CONTROL
    ? REALTIME_CONTROL_PERIOD_TICKS
    : REALTIME_TELEMETRY_PERIOD_TICKS;
}

static uint32_t deadline_for(real_time_task_t task) {
  return task == REALTIME_TASK_CONTROL
    ? REALTIME_CONTROL_DEADLINE_TICKS
    : REALTIME_TELEMETRY_DEADLINE_TICKS;
}

static uint32_t max_jitter_for(real_time_task_t task) {
  return task == REALTIME_TASK_CONTROL
    ? REALTIME_CONTROL_MAX_JITTER_TICKS
    : REALTIME_TELEMETRY_MAX_JITTER_TICKS;
}

static uint32_t execution_budget_for(real_time_task_t task) {
  return task == REALTIME_TASK_CONTROL
    ? REALTIME_CONTROL_EXECUTION_BUDGET_TICKS
    : REALTIME_TELEMETRY_EXECUTION_BUDGET_TICKS;
}

static void clear_active(real_time_guard_t *guard) {
  guard->active_task = REALTIME_TASK_COUNT;
  guard->active = false;
}

static real_time_status_t dispatch_task(
  real_time_guard_t *guard,
  real_time_task_t task,
  uint32_t *next_release,
  uint32_t now_ticks
) {
  const uint32_t nominal_release = *next_release;
  const uint32_t jitter = now_ticks - nominal_release;
  rtos_status_t status;

  if (!tick_has_reached(now_ticks, nominal_release)) {
    return REALTIME_STATUS_IDLE;
  }
  if (jitter > max_jitter_for(task)) {
    realtime_rtos_report_violation(
      task,
      REALTIME_VIOLATION_JITTER,
      nominal_release,
      now_ticks
    );
    *next_release = now_ticks + period_for(task);
    return REALTIME_STATUS_JITTER_VIOLATION;
  }

  status = realtime_rtos_begin(
    task,
    nominal_release,
    nominal_release + deadline_for(task)
  );
  if (status != RTOS_STATUS_OK) return REALTIME_STATUS_RTOS_ERROR;

  *next_release = now_ticks + period_for(task);
  guard->active_release = nominal_release;
  guard->active_started_at = now_ticks;
  guard->active_deadline = nominal_release + deadline_for(task);
  guard->active_task = task;
  guard->active = true;
  return REALTIME_STATUS_OK;
}

bool real_time_guard_init(real_time_guard_t *guard, uint32_t start_ticks) {
  if (guard == NULL) return false;

  *guard = (real_time_guard_t) {
    .next_control_release = start_ticks + REALTIME_CONTROL_PERIOD_TICKS,
    .next_telemetry_release = start_ticks + REALTIME_TELEMETRY_PERIOD_TICKS,
    .active_release = 0u,
    .active_started_at = 0u,
    .active_deadline = 0u,
    .active_task = REALTIME_TASK_COUNT,
    .active = false,
    .initialized = true,
  };
  return true;
}

real_time_status_t real_time_guard_dispatch(
  real_time_guard_t *guard,
  uint32_t now_ticks
) {
  real_time_status_t status;

  if (guard == NULL || !guard->initialized) {
    return REALTIME_STATUS_INVALID_ARGUMENT;
  }
  if (guard->active) return REALTIME_STATUS_BUSY;

  status = dispatch_task(
    guard,
    REALTIME_TASK_CONTROL,
    &guard->next_control_release,
    now_ticks
  );
  if (status != REALTIME_STATUS_IDLE) return status;
  return dispatch_task(
    guard,
    REALTIME_TASK_TELEMETRY,
    &guard->next_telemetry_release,
    now_ticks
  );
}

real_time_status_t real_time_guard_complete(
  real_time_guard_t *guard,
  uint32_t now_ticks
) {
  uint32_t elapsed;
  rtos_status_t status;

  if (guard == NULL || !guard->initialized) {
    return REALTIME_STATUS_INVALID_ARGUMENT;
  }
  if (!guard->active) return REALTIME_STATUS_IDLE;
  if (!tick_has_reached(now_ticks, guard->active_started_at)) {
    return REALTIME_STATUS_INVALID_ARGUMENT;
  }
  elapsed = now_ticks - guard->active_started_at;
  if (elapsed > execution_budget_for(guard->active_task)) {
    realtime_rtos_report_violation(
      guard->active_task,
      REALTIME_VIOLATION_BUDGET,
      guard->active_release,
      now_ticks
    );
    clear_active(guard);
    return REALTIME_STATUS_BUDGET_VIOLATION;
  }
  if (tick_is_after(now_ticks, guard->active_deadline)) {
    realtime_rtos_report_violation(
      guard->active_task,
      REALTIME_VIOLATION_DEADLINE,
      guard->active_release,
      now_ticks
    );
    clear_active(guard);
    return REALTIME_STATUS_DEADLINE_VIOLATION;
  }

  status = realtime_rtos_finish(guard->active_task);
  if (status != RTOS_STATUS_OK) return REALTIME_STATUS_RTOS_ERROR;
  clear_active(guard);
  return REALTIME_STATUS_OK;
}

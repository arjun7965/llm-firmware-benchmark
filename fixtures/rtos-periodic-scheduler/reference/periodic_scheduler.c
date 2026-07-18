#include "periodic_scheduler.h"

#include <stddef.h>

static bool tick_has_reached(uint32_t now_ticks, uint32_t release_ticks) {
  return (uint32_t) (now_ticks - release_ticks) < UINT32_C(0x80000000);
}

static rtos_status_t release_if_due(
  uint32_t *next_release,
  uint32_t now_ticks,
  uint32_t period_ticks,
  uint32_t deadline_ticks,
  rtos_scheduled_task_t task
) {
  if (!tick_has_reached(now_ticks, *next_release)) return RTOS_STATUS_OK;

  const rtos_status_t status = rtos_schedule_release(
    task,
    now_ticks + deadline_ticks
  );
  if (status != RTOS_STATUS_OK) return status;

  *next_release = now_ticks + period_ticks;
  return RTOS_STATUS_OK;
}

bool periodic_scheduler_init(periodic_scheduler_t *scheduler, uint32_t start_ticks) {
  if (scheduler == NULL) return false;

  scheduler->next_control_release = start_ticks + CONTROL_PERIOD_TICKS;
  scheduler->next_telemetry_release = start_ticks + TELEMETRY_PERIOD_TICKS;
  scheduler->initialized = true;
  return true;
}

rtos_status_t periodic_scheduler_dispatch(
  periodic_scheduler_t *scheduler,
  uint32_t now_ticks
) {
  if (scheduler == NULL || !scheduler->initialized) {
    return RTOS_STATUS_INVALID_ARGUMENT;
  }

  rtos_status_t status = release_if_due(
    &scheduler->next_control_release,
    now_ticks,
    CONTROL_PERIOD_TICKS,
    CONTROL_DEADLINE_TICKS,
    RTOS_SCHEDULED_TASK_CONTROL
  );
  if (status != RTOS_STATUS_OK) return status;

  return release_if_due(
    &scheduler->next_telemetry_release,
    now_ticks,
    TELEMETRY_PERIOD_TICKS,
    TELEMETRY_DEADLINE_TICKS,
    RTOS_SCHEDULED_TASK_TELEMETRY
  );
}

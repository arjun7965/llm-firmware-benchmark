#ifndef FIXTURE_REAL_TIME_GUARD_H
#define FIXTURE_REAL_TIME_GUARD_H

#include <stdint.h>

typedef enum {
  RTOS_STATUS_OK = 0,
  RTOS_STATUS_ERROR,
} rtos_status_t;

typedef enum {
  REALTIME_TASK_CONTROL = 0,
  REALTIME_TASK_TELEMETRY,
  REALTIME_TASK_COUNT,
} real_time_task_t;

typedef enum {
  REALTIME_VIOLATION_JITTER = 0,
  REALTIME_VIOLATION_BUDGET,
  REALTIME_VIOLATION_DEADLINE,
  REALTIME_VIOLATION_COUNT,
} real_time_violation_t;

rtos_status_t realtime_rtos_begin(
  real_time_task_t task,
  uint32_t nominal_release_ticks,
  uint32_t absolute_deadline_ticks
);
rtos_status_t realtime_rtos_finish(real_time_task_t task);
void realtime_rtos_report_violation(
  real_time_task_t task,
  real_time_violation_t violation,
  uint32_t nominal_release_ticks,
  uint32_t observed_ticks
);

#endif

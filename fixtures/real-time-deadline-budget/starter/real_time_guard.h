#ifndef REAL_TIME_GUARD_H
#define REAL_TIME_GUARD_H

#include <stdbool.h>
#include <stdint.h>

#include "fixture_real_time_guard.h"

#define REALTIME_CONTROL_PERIOD_TICKS UINT32_C(10)
#define REALTIME_CONTROL_DEADLINE_TICKS UINT32_C(6)
#define REALTIME_CONTROL_MAX_JITTER_TICKS UINT32_C(2)
#define REALTIME_CONTROL_EXECUTION_BUDGET_TICKS UINT32_C(5)
#define REALTIME_TELEMETRY_PERIOD_TICKS UINT32_C(25)
#define REALTIME_TELEMETRY_DEADLINE_TICKS UINT32_C(12)
#define REALTIME_TELEMETRY_MAX_JITTER_TICKS UINT32_C(4)
#define REALTIME_TELEMETRY_EXECUTION_BUDGET_TICKS UINT32_C(6)

typedef enum {
  REALTIME_STATUS_OK = 0,
  REALTIME_STATUS_IDLE,
  REALTIME_STATUS_INVALID_ARGUMENT,
  REALTIME_STATUS_BUSY,
  REALTIME_STATUS_JITTER_VIOLATION,
  REALTIME_STATUS_BUDGET_VIOLATION,
  REALTIME_STATUS_DEADLINE_VIOLATION,
  REALTIME_STATUS_RTOS_ERROR,
} real_time_status_t;

typedef struct {
  uint32_t next_control_release;
  uint32_t next_telemetry_release;
  uint32_t active_release;
  uint32_t active_started_at;
  uint32_t active_deadline;
  real_time_task_t active_task;
  bool active;
  bool initialized;
} real_time_guard_t;

bool real_time_guard_init(real_time_guard_t *guard, uint32_t start_ticks);
real_time_status_t real_time_guard_dispatch(
  real_time_guard_t *guard,
  uint32_t now_ticks
);
real_time_status_t real_time_guard_complete(
  real_time_guard_t *guard,
  uint32_t now_ticks
);

#endif

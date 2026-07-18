#ifndef FIXTURE_RTOS_SCHEDULER_H
#define FIXTURE_RTOS_SCHEDULER_H

#include <stdint.h>

typedef enum {
  RTOS_STATUS_OK = 0,
  RTOS_STATUS_INVALID_ARGUMENT,
  RTOS_STATUS_ERROR,
} rtos_status_t;

typedef enum {
  RTOS_SCHEDULED_TASK_CONTROL = 0,
  RTOS_SCHEDULED_TASK_TELEMETRY,
  RTOS_SCHEDULED_TASK_COUNT,
} rtos_scheduled_task_t;

rtos_status_t rtos_schedule_release(
  rtos_scheduled_task_t task,
  uint32_t absolute_deadline_ticks
);

#endif

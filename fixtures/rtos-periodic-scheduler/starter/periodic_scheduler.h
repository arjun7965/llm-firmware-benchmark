#ifndef PERIODIC_SCHEDULER_H
#define PERIODIC_SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

#include "fixture_rtos_scheduler.h"

#define CONTROL_PERIOD_TICKS UINT32_C(5)
#define TELEMETRY_PERIOD_TICKS UINT32_C(20)
#define CONTROL_DEADLINE_TICKS UINT32_C(2)
#define TELEMETRY_DEADLINE_TICKS UINT32_C(10)

typedef struct {
  uint32_t next_control_release;
  uint32_t next_telemetry_release;
  bool initialized;
} periodic_scheduler_t;

bool periodic_scheduler_init(periodic_scheduler_t *scheduler, uint32_t start_ticks);
rtos_status_t periodic_scheduler_dispatch(
  periodic_scheduler_t *scheduler,
  uint32_t now_ticks
);

#endif

#ifndef PRIORITY_INVERSION_H
#define PRIORITY_INVERSION_H

#include <stdbool.h>

#include "fixture_rtos.h"

#define SAFETY_LOCK_TIMEOUT_TICKS UINT32_C(2)

typedef struct {
  rtos_mutex_t *mutex;
  bool initialized;
} priority_guard_t;

bool priority_guard_init(priority_guard_t *guard);
rtos_status_t priority_guard_lock_telemetry(priority_guard_t *guard);
rtos_status_t priority_guard_lock_safety(priority_guard_t *guard);
rtos_status_t priority_guard_unlock(priority_guard_t *guard);

#endif

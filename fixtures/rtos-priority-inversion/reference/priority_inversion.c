#include "priority_inversion.h"

#include <stddef.h>

static bool guard_is_initialized(const priority_guard_t *guard) {
  return guard != NULL && guard->initialized && guard->mutex != NULL;
}

static rtos_status_t lock_guard(
  priority_guard_t *guard,
  uint32_t timeout_ticks
) {
  if (!guard_is_initialized(guard)) return RTOS_STATUS_INVALID_ARGUMENT;
  return rtos_mutex_lock(guard->mutex, timeout_ticks);
}

bool priority_guard_init(priority_guard_t *guard) {
  if (guard == NULL) return false;
  if (guard->initialized && guard->mutex != NULL) return true;

  guard->mutex = NULL;
  guard->initialized = false;
  rtos_mutex_t *mutex = rtos_mutex_create(true);
  if (mutex == NULL) return false;

  guard->mutex = mutex;
  guard->initialized = true;
  return true;
}

rtos_status_t priority_guard_lock_telemetry(priority_guard_t *guard) {
  return lock_guard(guard, RTOS_WAIT_FOREVER);
}

rtos_status_t priority_guard_lock_safety(priority_guard_t *guard) {
  return lock_guard(guard, SAFETY_LOCK_TIMEOUT_TICKS);
}

rtos_status_t priority_guard_unlock(priority_guard_t *guard) {
  if (!guard_is_initialized(guard)) return RTOS_STATUS_INVALID_ARGUMENT;
  return rtos_mutex_unlock(guard->mutex);
}

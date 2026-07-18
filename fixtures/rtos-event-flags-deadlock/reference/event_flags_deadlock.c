#include "event_flags_deadlock.h"

#include <stddef.h>

static bool supervisor_is_valid(const supervisor_coordination_t *supervisor) {
  return supervisor != NULL && supervisor->initialized &&
    supervisor->events != NULL && supervisor->configuration != NULL &&
    supervisor->actuator != NULL;
}

bool supervisor_coordination_init(supervisor_coordination_t *supervisor) {
  if (supervisor == NULL) return false;
  if (supervisor_is_valid(supervisor)) return true;

  supervisor->events = NULL;
  supervisor->configuration = NULL;
  supervisor->actuator = NULL;
  supervisor->initialized = false;
  rtos_event_flags_t *events = rtos_event_flags_create();
  if (events == NULL) return false;

  rtos_mutex_t *configuration = rtos_mutex_create();
  if (configuration == NULL) return false;
  rtos_mutex_t *actuator = rtos_mutex_create();
  if (actuator == NULL) return false;

  supervisor->events = events;
  supervisor->configuration = configuration;
  supervisor->actuator = actuator;
  supervisor->initialized = true;
  return true;
}

rtos_status_t supervisor_signal(
  supervisor_coordination_t *supervisor,
  uint32_t event_bits
) {
  if (
    !supervisor_is_valid(supervisor) ||
    event_bits == 0u ||
    (event_bits & ~SUPERVISOR_EVENT_MASK) != 0u
  ) {
    return RTOS_STATUS_INVALID_ARGUMENT;
  }
  return rtos_event_flags_set(supervisor->events, event_bits);
}

rtos_status_t supervisor_wait(
  supervisor_coordination_t *supervisor,
  uint32_t *received_bits
) {
  if (!supervisor_is_valid(supervisor) || received_bits == NULL) {
    return RTOS_STATUS_INVALID_ARGUMENT;
  }
  return rtos_event_flags_wait(
    supervisor->events,
    SUPERVISOR_EVENT_MASK,
    false,
    true,
    SUPERVISOR_EVENT_WAIT_TICKS,
    received_bits
  );
}

rtos_status_t supervisor_apply_configuration(
  supervisor_coordination_t *supervisor,
  uint32_t generation
) {
  if (!supervisor_is_valid(supervisor)) return RTOS_STATUS_INVALID_ARGUMENT;

  rtos_status_t status = rtos_mutex_lock(
    supervisor->configuration,
    CONFIGURATION_LOCK_TIMEOUT_TICKS
  );
  if (status != RTOS_STATUS_OK) return status;

  status = rtos_mutex_lock(
    supervisor->actuator,
    CONFIGURATION_LOCK_TIMEOUT_TICKS
  );
  if (status != RTOS_STATUS_OK) {
    (void) rtos_mutex_unlock(supervisor->configuration);
    return status;
  }

  const rtos_status_t apply_status = rtos_configuration_apply(generation);
  const rtos_status_t actuator_unlock_status = rtos_mutex_unlock(
    supervisor->actuator
  );
  const rtos_status_t configuration_unlock_status = rtos_mutex_unlock(
    supervisor->configuration
  );
  if (apply_status != RTOS_STATUS_OK) return apply_status;
  if (actuator_unlock_status != RTOS_STATUS_OK) return actuator_unlock_status;
  return configuration_unlock_status;
}

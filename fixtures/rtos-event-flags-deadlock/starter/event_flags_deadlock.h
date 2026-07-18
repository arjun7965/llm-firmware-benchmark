#ifndef EVENT_FLAGS_DEADLOCK_H
#define EVENT_FLAGS_DEADLOCK_H

#include <stdbool.h>
#include <stdint.h>

#include "fixture_rtos_events.h"

#define SUPERVISOR_EVENT_SAMPLE_READY UINT32_C(1)
#define SUPERVISOR_EVENT_STOP_REQUESTED UINT32_C(2)
#define SUPERVISOR_EVENT_FAULT UINT32_C(4)
#define SUPERVISOR_EVENT_MASK \
  (SUPERVISOR_EVENT_SAMPLE_READY | SUPERVISOR_EVENT_STOP_REQUESTED | \
    SUPERVISOR_EVENT_FAULT)
#define SUPERVISOR_EVENT_WAIT_TICKS UINT32_C(2)
#define CONFIGURATION_LOCK_TIMEOUT_TICKS UINT32_C(1)

typedef struct {
  rtos_event_flags_t *events;
  rtos_mutex_t *configuration;
  rtos_mutex_t *actuator;
  bool initialized;
} supervisor_coordination_t;

bool supervisor_coordination_init(supervisor_coordination_t *supervisor);
rtos_status_t supervisor_signal(
  supervisor_coordination_t *supervisor,
  uint32_t event_bits
);
rtos_status_t supervisor_wait(
  supervisor_coordination_t *supervisor,
  uint32_t *received_bits
);
rtos_status_t supervisor_apply_configuration(
  supervisor_coordination_t *supervisor,
  uint32_t generation
);

#endif

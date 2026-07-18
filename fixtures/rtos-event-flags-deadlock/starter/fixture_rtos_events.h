#ifndef FIXTURE_RTOS_EVENTS_H
#define FIXTURE_RTOS_EVENTS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct rtos_event_flags rtos_event_flags_t;
typedef struct rtos_mutex rtos_mutex_t;

typedef enum {
  RTOS_STATUS_OK = 0,
  RTOS_STATUS_TIMEOUT,
  RTOS_STATUS_NOT_OWNER,
  RTOS_STATUS_INVALID_ARGUMENT,
  RTOS_STATUS_ERROR,
} rtos_status_t;

#define RTOS_WAIT_FOREVER UINT32_MAX

rtos_event_flags_t *rtos_event_flags_create(void);
rtos_mutex_t *rtos_mutex_create(void);
rtos_status_t rtos_event_flags_set(rtos_event_flags_t *events, uint32_t bits);
rtos_status_t rtos_event_flags_wait(
  rtos_event_flags_t *events,
  uint32_t bits_to_wait,
  bool wait_all,
  bool clear_on_exit,
  uint32_t timeout_ticks,
  uint32_t *received_bits
);
rtos_status_t rtos_mutex_lock(rtos_mutex_t *mutex, uint32_t timeout_ticks);
rtos_status_t rtos_mutex_unlock(rtos_mutex_t *mutex);
rtos_status_t rtos_configuration_apply(uint32_t generation);

#endif

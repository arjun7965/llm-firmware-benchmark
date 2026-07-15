#ifndef FIXTURE_RTOS_H
#define FIXTURE_RTOS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct rtos_mutex rtos_mutex_t;

typedef enum {
  RTOS_STATUS_OK = 0,
  RTOS_STATUS_BLOCKED,
  RTOS_STATUS_TIMEOUT,
  RTOS_STATUS_NOT_OWNER,
  RTOS_STATUS_INVALID_ARGUMENT,
  RTOS_STATUS_ERROR,
} rtos_status_t;

#define RTOS_WAIT_FOREVER UINT32_MAX

rtos_mutex_t *rtos_mutex_create(bool priority_inheritance);
rtos_status_t rtos_mutex_lock(rtos_mutex_t *mutex, uint32_t timeout_ticks);
rtos_status_t rtos_mutex_unlock(rtos_mutex_t *mutex);

#endif

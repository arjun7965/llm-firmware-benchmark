#ifndef FIXTURE_RTOS_QUEUE_H
#define FIXTURE_RTOS_QUEUE_H

#include <stddef.h>
#include <stdint.h>

typedef struct rtos_queue rtos_queue_t;
typedef struct rtos_semaphore rtos_semaphore_t;

typedef enum {
  RTOS_STATUS_OK = 0,
  RTOS_STATUS_FULL,
  RTOS_STATUS_EMPTY,
  RTOS_STATUS_TIMEOUT,
  RTOS_STATUS_INVALID_ARGUMENT,
  RTOS_STATUS_ERROR,
} rtos_status_t;

#define RTOS_WAIT_FOREVER UINT32_MAX

rtos_queue_t *rtos_queue_create(size_t item_size, size_t capacity);
rtos_semaphore_t *rtos_semaphore_create_counting(
  uint32_t maximum_count,
  uint32_t initial_count
);
rtos_status_t rtos_queue_send(
  rtos_queue_t *queue,
  const void *item,
  uint32_t timeout_ticks
);
rtos_status_t rtos_queue_receive(
  rtos_queue_t *queue,
  void *item,
  uint32_t timeout_ticks
);
rtos_status_t rtos_semaphore_give(rtos_semaphore_t *semaphore);
rtos_status_t rtos_semaphore_take(
  rtos_semaphore_t *semaphore,
  uint32_t timeout_ticks
);

#endif

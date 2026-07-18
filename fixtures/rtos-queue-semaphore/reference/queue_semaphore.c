#include "queue_semaphore.h"

#include <stddef.h>

static bool pipeline_is_valid(const sensor_pipeline_t *pipeline) {
  return pipeline != NULL && pipeline->initialized &&
    pipeline->queue != NULL && pipeline->available != NULL;
}

bool sensor_pipeline_init(sensor_pipeline_t *pipeline) {
  if (pipeline == NULL) return false;
  if (pipeline_is_valid(pipeline)) return true;

  pipeline->queue = NULL;
  pipeline->available = NULL;
  pipeline->initialized = false;
  rtos_queue_t *queue = rtos_queue_create(
    sizeof(sensor_sample_t),
    SENSOR_QUEUE_CAPACITY
  );
  if (queue == NULL) return false;

  rtos_semaphore_t *available = rtos_semaphore_create_counting(
    SENSOR_QUEUE_CAPACITY,
    0u
  );
  if (available == NULL) return false;

  pipeline->queue = queue;
  pipeline->available = available;
  pipeline->initialized = true;
  return true;
}

rtos_status_t sensor_pipeline_publish(
  sensor_pipeline_t *pipeline,
  const sensor_sample_t *sample
) {
  if (!pipeline_is_valid(pipeline) || sample == NULL) {
    return RTOS_STATUS_INVALID_ARGUMENT;
  }

  const rtos_status_t status = rtos_queue_send(pipeline->queue, sample, 0u);
  if (status != RTOS_STATUS_OK) return status;
  return rtos_semaphore_give(pipeline->available);
}

rtos_status_t sensor_pipeline_wait_and_take(
  sensor_pipeline_t *pipeline,
  sensor_sample_t *sample
) {
  if (!pipeline_is_valid(pipeline) || sample == NULL) {
    return RTOS_STATUS_INVALID_ARGUMENT;
  }

  const rtos_status_t status = rtos_semaphore_take(
    pipeline->available,
    WORKER_WAIT_TIMEOUT_TICKS
  );
  if (status != RTOS_STATUS_OK) return status;
  return rtos_queue_receive(pipeline->queue, sample, 0u);
}

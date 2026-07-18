#ifndef QUEUE_SEMAPHORE_H
#define QUEUE_SEMAPHORE_H

#include <stdbool.h>
#include <stdint.h>

#include "fixture_rtos_queue.h"

#define SENSOR_QUEUE_CAPACITY UINT32_C(4)
#define WORKER_WAIT_TIMEOUT_TICKS UINT32_C(3)

typedef struct {
  uint16_t sequence;
  int16_t temperature_milli_c;
} sensor_sample_t;

typedef struct {
  rtos_queue_t *queue;
  rtos_semaphore_t *available;
  bool initialized;
} sensor_pipeline_t;

bool sensor_pipeline_init(sensor_pipeline_t *pipeline);
rtos_status_t sensor_pipeline_publish(
  sensor_pipeline_t *pipeline,
  const sensor_sample_t *sample
);
rtos_status_t sensor_pipeline_wait_and_take(
  sensor_pipeline_t *pipeline,
  sensor_sample_t *sample
);

#endif

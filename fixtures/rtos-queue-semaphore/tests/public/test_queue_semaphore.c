#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "mock_rtos_queue.h"
#include "queue_semaphore.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
              __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

static sensor_sample_t sample(uint16_t sequence, int16_t temperature) {
  return (sensor_sample_t) {
    .sequence = sequence,
    .temperature_milli_c = temperature,
  };
}

static bool samples_equal(sensor_sample_t left, sensor_sample_t right) {
  return left.sequence == right.sequence &&
    left.temperature_milli_c == right.temperature_milli_c;
}

static bool test_initialization_and_creation_failure(void) {
  sensor_pipeline_t pipeline = { 0 };

  mock_rtos_queue_reset();
  CHECK(!sensor_pipeline_init(NULL));
  CHECK(mock_rtos_queue_create_count() == 0u);
  CHECK(mock_rtos_semaphore_create_count() == 0u);

  CHECK(sensor_pipeline_init(&pipeline));
  CHECK(pipeline.initialized);
  CHECK(pipeline.queue != NULL);
  CHECK(pipeline.available != NULL);
  CHECK(mock_rtos_queue_create_count() == 1u);
  CHECK(mock_rtos_queue_item_size() == sizeof(sensor_sample_t));
  CHECK(mock_rtos_queue_capacity() == SENSOR_QUEUE_CAPACITY);
  CHECK(mock_rtos_semaphore_create_count() == 1u);
  CHECK(mock_rtos_semaphore_maximum_count() == SENSOR_QUEUE_CAPACITY);
  CHECK(mock_rtos_semaphore_initial_count() == 0u);
  CHECK(sensor_pipeline_init(&pipeline));
  CHECK(mock_rtos_queue_create_count() == 1u);
  CHECK(mock_rtos_semaphore_create_count() == 1u);

  pipeline = (sensor_pipeline_t) { 0 };
  mock_rtos_queue_reset();
  mock_rtos_queue_fail_next_queue_create();
  CHECK(!sensor_pipeline_init(&pipeline));
  CHECK(pipeline.queue == NULL);
  CHECK(pipeline.available == NULL);
  CHECK(!pipeline.initialized);
  CHECK(mock_rtos_queue_create_count() == 1u);
  CHECK(mock_rtos_semaphore_create_count() == 0u);

  pipeline = (sensor_pipeline_t) { 0 };
  pipeline.queue = (rtos_queue_t *) (uintptr_t) 1u;
  pipeline.available = (rtos_semaphore_t *) (uintptr_t) 1u;
  pipeline.initialized = false;
  mock_rtos_queue_reset();
  mock_rtos_queue_fail_next_semaphore_create();
  CHECK(!sensor_pipeline_init(&pipeline));
  CHECK(pipeline.queue == NULL);
  CHECK(pipeline.available == NULL);
  CHECK(!pipeline.initialized);
  CHECK(mock_rtos_queue_create_count() == 1u);
  CHECK(mock_rtos_semaphore_create_count() == 1u);
  return true;
}

static bool test_invalid_pipeline_makes_no_rtos_calls(void) {
  sensor_pipeline_t pipeline = { 0 };
  sensor_sample_t value = sample(1u, 25000);

  mock_rtos_queue_reset();
  CHECK(sensor_pipeline_publish(NULL, &value) == RTOS_STATUS_INVALID_ARGUMENT);
  CHECK(sensor_pipeline_wait_and_take(NULL, &value) == RTOS_STATUS_INVALID_ARGUMENT);
  CHECK(sensor_pipeline_publish(&pipeline, NULL) == RTOS_STATUS_INVALID_ARGUMENT);
  CHECK(sensor_pipeline_wait_and_take(&pipeline, NULL) == RTOS_STATUS_INVALID_ARGUMENT);
  CHECK(sensor_pipeline_publish(&pipeline, &value) == RTOS_STATUS_INVALID_ARGUMENT);
  CHECK(sensor_pipeline_wait_and_take(&pipeline, &value) == RTOS_STATUS_INVALID_ARGUMENT);
  CHECK(mock_rtos_queue_send_count() == 0u);
  CHECK(mock_rtos_queue_receive_count() == 0u);
  CHECK(mock_rtos_semaphore_give_count() == 0u);
  CHECK(mock_rtos_semaphore_take_count() == 0u);

  mock_rtos_queue_reset();
  CHECK(sensor_pipeline_init(&pipeline));
  pipeline.initialized = false;
  CHECK(sensor_pipeline_publish(&pipeline, &value) == RTOS_STATUS_INVALID_ARGUMENT);
  CHECK(mock_rtos_queue_send_count() == 0u);
  return true;
}

static bool test_fifo_token_order_and_bounded_wait(void) {
  sensor_pipeline_t pipeline = { 0 };
  sensor_sample_t first = sample(7u, 25125);
  sensor_sample_t second = sample(8u, -3000);
  sensor_sample_t result = { 0 };

  mock_rtos_queue_reset();
  CHECK(sensor_pipeline_init(&pipeline));
  CHECK(sensor_pipeline_publish(&pipeline, &first) == RTOS_STATUS_OK);
  CHECK(sensor_pipeline_publish(&pipeline, &second) == RTOS_STATUS_OK);
  CHECK(mock_rtos_queue_item_count() == 2u);
  CHECK(mock_rtos_semaphore_count() == 2u);
  CHECK(mock_rtos_last_send_timeout() == 0u);
  CHECK(mock_rtos_operation_count() == 4u);
  CHECK(mock_rtos_operation(0u) == MOCK_RTOS_OPERATION_QUEUE_SEND);
  CHECK(mock_rtos_operation(1u) == MOCK_RTOS_OPERATION_SEMAPHORE_GIVE);
  CHECK(mock_rtos_operation(2u) == MOCK_RTOS_OPERATION_QUEUE_SEND);
  CHECK(mock_rtos_operation(3u) == MOCK_RTOS_OPERATION_SEMAPHORE_GIVE);

  CHECK(sensor_pipeline_wait_and_take(&pipeline, &result) == RTOS_STATUS_OK);
  CHECK(samples_equal(result, first));
  CHECK(mock_rtos_last_take_timeout() == WORKER_WAIT_TIMEOUT_TICKS);
  CHECK(mock_rtos_last_receive_timeout() == 0u);
  CHECK(mock_rtos_operation(4u) == MOCK_RTOS_OPERATION_SEMAPHORE_TAKE);
  CHECK(mock_rtos_operation(5u) == MOCK_RTOS_OPERATION_QUEUE_RECEIVE);
  CHECK(sensor_pipeline_wait_and_take(&pipeline, &result) == RTOS_STATUS_OK);
  CHECK(samples_equal(result, second));
  CHECK(mock_rtos_queue_item_count() == 0u);
  CHECK(mock_rtos_semaphore_count() == 0u);
  return true;
}

static bool test_capacity_and_failure_propagation(void) {
  sensor_pipeline_t pipeline = { 0 };
  sensor_sample_t value = sample(1u, 1);
  sensor_sample_t output = { 0 };

  mock_rtos_queue_reset();
  CHECK(sensor_pipeline_init(&pipeline));
  for (uint16_t index = 0u; index < SENSOR_QUEUE_CAPACITY; index++) {
    value.sequence = index;
    CHECK(sensor_pipeline_publish(&pipeline, &value) == RTOS_STATUS_OK);
  }
  CHECK(sensor_pipeline_publish(&pipeline, &value) == RTOS_STATUS_FULL);
  CHECK(mock_rtos_queue_item_count() == SENSOR_QUEUE_CAPACITY);
  CHECK(mock_rtos_semaphore_count() == SENSOR_QUEUE_CAPACITY);
  CHECK(mock_rtos_semaphore_give_count() == SENSOR_QUEUE_CAPACITY);

  mock_rtos_queue_force_next_send_status(RTOS_STATUS_ERROR);
  CHECK(sensor_pipeline_publish(&pipeline, &value) == RTOS_STATUS_ERROR);
  CHECK(mock_rtos_semaphore_give_count() == SENSOR_QUEUE_CAPACITY);

  mock_rtos_queue_reset();
  pipeline = (sensor_pipeline_t) { 0 };
  CHECK(sensor_pipeline_init(&pipeline));
  CHECK(sensor_pipeline_wait_and_take(&pipeline, &output) == RTOS_STATUS_TIMEOUT);
  CHECK(mock_rtos_queue_receive_count() == 0u);
  mock_rtos_queue_force_next_take_status(RTOS_STATUS_ERROR);
  CHECK(sensor_pipeline_wait_and_take(&pipeline, &output) == RTOS_STATUS_ERROR);
  CHECK(mock_rtos_queue_receive_count() == 0u);
  return true;
}

static bool test_give_and_receive_failures_are_visible(void) {
  sensor_pipeline_t pipeline = { 0 };
  sensor_sample_t value = sample(4u, 42);
  sensor_sample_t output = { 0 };

  mock_rtos_queue_reset();
  CHECK(sensor_pipeline_init(&pipeline));
  mock_rtos_queue_force_next_give_status(RTOS_STATUS_ERROR);
  CHECK(sensor_pipeline_publish(&pipeline, &value) == RTOS_STATUS_ERROR);
  CHECK(mock_rtos_queue_item_count() == 1u);
  CHECK(mock_rtos_semaphore_count() == 0u);

  mock_rtos_queue_reset();
  pipeline = (sensor_pipeline_t) { 0 };
  CHECK(sensor_pipeline_init(&pipeline));
  CHECK(sensor_pipeline_publish(&pipeline, &value) == RTOS_STATUS_OK);
  mock_rtos_queue_force_next_receive_status(RTOS_STATUS_ERROR);
  CHECK(sensor_pipeline_wait_and_take(&pipeline, &output) == RTOS_STATUS_ERROR);
  CHECK(mock_rtos_semaphore_count() == 0u);
  CHECK(mock_rtos_queue_item_count() == 1u);
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "initialization and creation failure", test_initialization_and_creation_failure },
    { "invalid pipeline", test_invalid_pipeline_makes_no_rtos_calls },
    { "FIFO token order", test_fifo_token_order_and_bounded_wait },
    { "capacity and failures", test_capacity_and_failure_propagation },
    { "give and receive failure", test_give_and_receive_failures_are_visible },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) return 1;
    printf("ok - %s\n", tests[index].name);
  }
  return 0;
}

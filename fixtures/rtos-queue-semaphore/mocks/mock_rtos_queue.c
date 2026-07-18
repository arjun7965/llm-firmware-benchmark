#include "mock_rtos_queue.h"

#include <stdbool.h>
#include <string.h>

#include "queue_semaphore.h"

#define MOCK_RTOS_OPERATION_CAPACITY 32u

struct rtos_queue {
  size_t item_size;
  size_t capacity;
  sensor_sample_t items[SENSOR_QUEUE_CAPACITY];
  size_t head;
  size_t tail;
  size_t count;
};

struct rtos_semaphore {
  uint32_t maximum_count;
  uint32_t count;
};

static struct rtos_queue queue_storage;
static struct rtos_semaphore semaphore_storage;
static bool queue_created;
static bool semaphore_created;
static bool fail_next_queue_create;
static bool fail_next_semaphore_create;
static bool force_next_send_status;
static bool force_next_give_status;
static bool force_next_take_status;
static bool force_next_receive_status;
static rtos_status_t next_send_status;
static rtos_status_t next_give_status;
static rtos_status_t next_take_status;
static rtos_status_t next_receive_status;
static size_t queue_create_count;
static size_t semaphore_create_count;
static size_t queue_send_count;
static size_t queue_receive_count;
static size_t semaphore_give_count;
static size_t semaphore_take_count;
static uint32_t last_send_timeout;
static uint32_t last_receive_timeout;
static uint32_t last_take_timeout;
static mock_rtos_operation_t operations[MOCK_RTOS_OPERATION_CAPACITY];
static size_t operation_count;

static bool queue_is_valid(const rtos_queue_t *queue) {
  return queue_created && queue == &queue_storage;
}

static bool semaphore_is_valid(const rtos_semaphore_t *semaphore) {
  return semaphore_created && semaphore == &semaphore_storage;
}

static void record_operation(mock_rtos_operation_t operation) {
  if (operation_count < MOCK_RTOS_OPERATION_CAPACITY) {
    operations[operation_count++] = operation;
  }
}

void mock_rtos_queue_reset(void) {
  memset(&queue_storage, 0, sizeof(queue_storage));
  memset(&semaphore_storage, 0, sizeof(semaphore_storage));
  queue_created = false;
  semaphore_created = false;
  fail_next_queue_create = false;
  fail_next_semaphore_create = false;
  force_next_send_status = false;
  force_next_give_status = false;
  force_next_take_status = false;
  force_next_receive_status = false;
  next_send_status = RTOS_STATUS_OK;
  next_give_status = RTOS_STATUS_OK;
  next_take_status = RTOS_STATUS_OK;
  next_receive_status = RTOS_STATUS_OK;
  queue_create_count = 0u;
  semaphore_create_count = 0u;
  queue_send_count = 0u;
  queue_receive_count = 0u;
  semaphore_give_count = 0u;
  semaphore_take_count = 0u;
  last_send_timeout = 0u;
  last_receive_timeout = 0u;
  last_take_timeout = 0u;
  operation_count = 0u;
}

void mock_rtos_queue_fail_next_queue_create(void) {
  fail_next_queue_create = true;
}

void mock_rtos_queue_fail_next_semaphore_create(void) {
  fail_next_semaphore_create = true;
}

void mock_rtos_queue_force_next_send_status(rtos_status_t status) {
  force_next_send_status = true;
  next_send_status = status;
}

void mock_rtos_queue_force_next_give_status(rtos_status_t status) {
  force_next_give_status = true;
  next_give_status = status;
}

void mock_rtos_queue_force_next_take_status(rtos_status_t status) {
  force_next_take_status = true;
  next_take_status = status;
}

void mock_rtos_queue_force_next_receive_status(rtos_status_t status) {
  force_next_receive_status = true;
  next_receive_status = status;
}

size_t mock_rtos_queue_create_count(void) { return queue_create_count; }
size_t mock_rtos_semaphore_create_count(void) { return semaphore_create_count; }
size_t mock_rtos_queue_item_size(void) { return queue_storage.item_size; }
size_t mock_rtos_queue_capacity(void) { return queue_storage.capacity; }
uint32_t mock_rtos_semaphore_maximum_count(void) {
  return semaphore_storage.maximum_count;
}
uint32_t mock_rtos_semaphore_initial_count(void) { return semaphore_storage.count; }
size_t mock_rtos_queue_send_count(void) { return queue_send_count; }
size_t mock_rtos_queue_receive_count(void) { return queue_receive_count; }
size_t mock_rtos_semaphore_give_count(void) { return semaphore_give_count; }
size_t mock_rtos_semaphore_take_count(void) { return semaphore_take_count; }
uint32_t mock_rtos_last_send_timeout(void) { return last_send_timeout; }
uint32_t mock_rtos_last_receive_timeout(void) { return last_receive_timeout; }
uint32_t mock_rtos_last_take_timeout(void) { return last_take_timeout; }
size_t mock_rtos_queue_item_count(void) { return queue_storage.count; }
uint32_t mock_rtos_semaphore_count(void) { return semaphore_storage.count; }
size_t mock_rtos_operation_count(void) { return operation_count; }
mock_rtos_operation_t mock_rtos_operation(size_t index) {
  return index < operation_count
    ? operations[index]
    : MOCK_RTOS_OPERATION_QUEUE_SEND;
}

rtos_queue_t *rtos_queue_create(size_t item_size, size_t capacity) {
  queue_create_count++;
  if (
    fail_next_queue_create ||
    queue_created ||
    item_size != sizeof(sensor_sample_t) ||
    capacity != SENSOR_QUEUE_CAPACITY
  ) {
    fail_next_queue_create = false;
    return NULL;
  }
  queue_storage.item_size = item_size;
  queue_storage.capacity = capacity;
  queue_created = true;
  return &queue_storage;
}

rtos_semaphore_t *rtos_semaphore_create_counting(
  uint32_t maximum_count,
  uint32_t initial_count
) {
  semaphore_create_count++;
  if (
    fail_next_semaphore_create ||
    semaphore_created ||
    maximum_count != SENSOR_QUEUE_CAPACITY ||
    initial_count != 0u
  ) {
    fail_next_semaphore_create = false;
    return NULL;
  }
  semaphore_storage.maximum_count = maximum_count;
  semaphore_storage.count = initial_count;
  semaphore_created = true;
  return &semaphore_storage;
}

rtos_status_t rtos_queue_send(
  rtos_queue_t *queue,
  const void *item,
  uint32_t timeout_ticks
) {
  if (!queue_is_valid(queue) || item == NULL) return RTOS_STATUS_INVALID_ARGUMENT;
  record_operation(MOCK_RTOS_OPERATION_QUEUE_SEND);
  queue_send_count++;
  last_send_timeout = timeout_ticks;
  if (force_next_send_status) {
    force_next_send_status = false;
    return next_send_status;
  }
  if (queue_storage.count == queue_storage.capacity) return RTOS_STATUS_FULL;

  queue_storage.items[queue_storage.tail] = *(const sensor_sample_t *) item;
  queue_storage.tail = (queue_storage.tail + 1u) % queue_storage.capacity;
  queue_storage.count++;
  return RTOS_STATUS_OK;
}

rtos_status_t rtos_queue_receive(
  rtos_queue_t *queue,
  void *item,
  uint32_t timeout_ticks
) {
  if (!queue_is_valid(queue) || item == NULL) return RTOS_STATUS_INVALID_ARGUMENT;
  record_operation(MOCK_RTOS_OPERATION_QUEUE_RECEIVE);
  queue_receive_count++;
  last_receive_timeout = timeout_ticks;
  if (force_next_receive_status) {
    force_next_receive_status = false;
    return next_receive_status;
  }
  if (queue_storage.count == 0u) return RTOS_STATUS_EMPTY;

  *(sensor_sample_t *) item = queue_storage.items[queue_storage.head];
  queue_storage.head = (queue_storage.head + 1u) % queue_storage.capacity;
  queue_storage.count--;
  return RTOS_STATUS_OK;
}

rtos_status_t rtos_semaphore_give(rtos_semaphore_t *semaphore) {
  if (!semaphore_is_valid(semaphore)) return RTOS_STATUS_INVALID_ARGUMENT;
  record_operation(MOCK_RTOS_OPERATION_SEMAPHORE_GIVE);
  semaphore_give_count++;
  if (force_next_give_status) {
    force_next_give_status = false;
    return next_give_status;
  }
  if (semaphore_storage.count == semaphore_storage.maximum_count) {
    return RTOS_STATUS_FULL;
  }
  semaphore_storage.count++;
  return RTOS_STATUS_OK;
}

rtos_status_t rtos_semaphore_take(
  rtos_semaphore_t *semaphore,
  uint32_t timeout_ticks
) {
  if (!semaphore_is_valid(semaphore)) return RTOS_STATUS_INVALID_ARGUMENT;
  record_operation(MOCK_RTOS_OPERATION_SEMAPHORE_TAKE);
  semaphore_take_count++;
  last_take_timeout = timeout_ticks;
  if (force_next_take_status) {
    force_next_take_status = false;
    return next_take_status;
  }
  if (semaphore_storage.count == 0u) return RTOS_STATUS_TIMEOUT;

  semaphore_storage.count--;
  return RTOS_STATUS_OK;
}

#include "mock_rtos_events.h"

#include <string.h>

enum {
  MOCK_RTOS_MUTEX_CONFIGURATION = 0,
  MOCK_RTOS_MUTEX_ACTUATOR,
  MOCK_RTOS_MUTEX_COUNT,
  MOCK_RTOS_MAX_LOCKS = 16,
  MOCK_RTOS_MAX_OPERATIONS = 32,
};

struct rtos_event_flags {
  uint32_t bits;
};

struct rtos_mutex {
  bool locked;
  size_t index;
};

static struct rtos_event_flags events_storage;
static struct rtos_mutex mutexes[MOCK_RTOS_MUTEX_COUNT];
static bool events_created;
static size_t created_mutex_count;
static bool fail_next_event_create;
static bool fail_next_mutex_create;
static bool force_next_lock_status;
static bool force_next_unlock_status;
static bool force_next_apply_status;
static size_t forced_unlock_call;
static rtos_status_t next_lock_status;
static rtos_status_t next_unlock_status;
static rtos_status_t next_apply_status;
static size_t event_create_count;
static size_t mutex_create_count;
static size_t event_set_count;
static size_t event_wait_count;
static size_t lock_count;
static size_t unlock_count;
static size_t apply_count;
static uint32_t last_event_set_bits;
static uint32_t last_wait_bits;
static bool last_wait_all;
static bool last_clear_on_exit;
static uint32_t last_wait_timeout;
static uint32_t lock_timeouts[MOCK_RTOS_MAX_LOCKS];
static mock_rtos_operation_t operations[MOCK_RTOS_MAX_OPERATIONS];
static size_t operation_count;

static bool events_are_valid(const rtos_event_flags_t *events) {
  return events_created && events == &events_storage;
}

static bool mutex_is_valid(const rtos_mutex_t *mutex) {
  return (mutex == &mutexes[MOCK_RTOS_MUTEX_CONFIGURATION] &&
    created_mutex_count > MOCK_RTOS_MUTEX_CONFIGURATION) ||
    (mutex == &mutexes[MOCK_RTOS_MUTEX_ACTUATOR] &&
      created_mutex_count > MOCK_RTOS_MUTEX_ACTUATOR);
}

static void record_operation(mock_rtos_operation_t operation) {
  if (operation_count < MOCK_RTOS_MAX_OPERATIONS) {
    operations[operation_count++] = operation;
  }
}

static mock_rtos_operation_t lock_operation(const rtos_mutex_t *mutex) {
  return mutex->index == MOCK_RTOS_MUTEX_CONFIGURATION
    ? MOCK_RTOS_OPERATION_CONFIGURATION_LOCK
    : MOCK_RTOS_OPERATION_ACTUATOR_LOCK;
}

static mock_rtos_operation_t unlock_operation(const rtos_mutex_t *mutex) {
  return mutex->index == MOCK_RTOS_MUTEX_CONFIGURATION
    ? MOCK_RTOS_OPERATION_CONFIGURATION_UNLOCK
    : MOCK_RTOS_OPERATION_ACTUATOR_UNLOCK;
}

void mock_rtos_events_reset(void) {
  memset(&events_storage, 0, sizeof(events_storage));
  memset(mutexes, 0, sizeof(mutexes));
  for (size_t index = 0u; index < MOCK_RTOS_MUTEX_COUNT; index++) {
    mutexes[index].index = index;
  }
  events_created = false;
  created_mutex_count = 0u;
  fail_next_event_create = false;
  fail_next_mutex_create = false;
  force_next_lock_status = false;
  force_next_unlock_status = false;
  force_next_apply_status = false;
  forced_unlock_call = 0u;
  next_lock_status = RTOS_STATUS_OK;
  next_unlock_status = RTOS_STATUS_OK;
  next_apply_status = RTOS_STATUS_OK;
  event_create_count = 0u;
  mutex_create_count = 0u;
  event_set_count = 0u;
  event_wait_count = 0u;
  lock_count = 0u;
  unlock_count = 0u;
  apply_count = 0u;
  last_event_set_bits = 0u;
  last_wait_bits = 0u;
  last_wait_all = false;
  last_clear_on_exit = false;
  last_wait_timeout = 0u;
  memset(lock_timeouts, 0, sizeof(lock_timeouts));
  operation_count = 0u;
}

void mock_rtos_events_fail_next_event_create(void) {
  fail_next_event_create = true;
}

void mock_rtos_events_fail_next_mutex_create(void) {
  fail_next_mutex_create = true;
}

void mock_rtos_events_force_next_lock_status(rtos_status_t status) {
  force_next_lock_status = true;
  next_lock_status = status;
}

void mock_rtos_events_force_next_unlock_status(rtos_status_t status) {
  force_next_unlock_status = true;
  forced_unlock_call = unlock_count + 1u;
  next_unlock_status = status;
}

void mock_rtos_events_force_unlock_status_on_call(
  size_t call_index,
  rtos_status_t status
) {
  force_next_unlock_status = true;
  forced_unlock_call = call_index;
  next_unlock_status = status;
}

void mock_rtos_events_force_next_apply_status(rtos_status_t status) {
  force_next_apply_status = true;
  next_apply_status = status;
}

void mock_rtos_events_lock_actuator_from_peer(void) {
  if (created_mutex_count > MOCK_RTOS_MUTEX_ACTUATOR) {
    mutexes[MOCK_RTOS_MUTEX_ACTUATOR].locked = true;
  }
}

void mock_rtos_events_unlock_actuator_from_peer(void) {
  if (created_mutex_count > MOCK_RTOS_MUTEX_ACTUATOR) {
    mutexes[MOCK_RTOS_MUTEX_ACTUATOR].locked = false;
  }
}

size_t mock_rtos_events_create_count(void) { return event_create_count; }
size_t mock_rtos_mutex_create_count(void) { return mutex_create_count; }
size_t mock_rtos_event_set_count(void) { return event_set_count; }
size_t mock_rtos_event_wait_count(void) { return event_wait_count; }
size_t mock_rtos_lock_count(void) { return lock_count; }
size_t mock_rtos_unlock_count(void) { return unlock_count; }
size_t mock_rtos_apply_count(void) { return apply_count; }
uint32_t mock_rtos_event_bits(void) { return events_storage.bits; }
uint32_t mock_rtos_last_event_set_bits(void) { return last_event_set_bits; }
uint32_t mock_rtos_last_wait_bits(void) { return last_wait_bits; }
bool mock_rtos_last_wait_all(void) { return last_wait_all; }
bool mock_rtos_last_clear_on_exit(void) { return last_clear_on_exit; }
uint32_t mock_rtos_last_wait_timeout(void) { return last_wait_timeout; }
uint32_t mock_rtos_lock_timeout(size_t index) {
  return index < lock_count ? lock_timeouts[index] : 0u;
}
bool mock_rtos_configuration_locked(void) {
  return mutexes[MOCK_RTOS_MUTEX_CONFIGURATION].locked;
}
bool mock_rtos_actuator_locked(void) {
  return mutexes[MOCK_RTOS_MUTEX_ACTUATOR].locked;
}
size_t mock_rtos_operation_count(void) { return operation_count; }
mock_rtos_operation_t mock_rtos_operation(size_t index) {
  return index < operation_count
    ? operations[index]
    : MOCK_RTOS_OPERATION_EVENT_SET;
}

rtos_event_flags_t *rtos_event_flags_create(void) {
  event_create_count++;
  if (fail_next_event_create || events_created) {
    fail_next_event_create = false;
    return NULL;
  }
  events_created = true;
  return &events_storage;
}

rtos_mutex_t *rtos_mutex_create(void) {
  mutex_create_count++;
  if (fail_next_mutex_create || created_mutex_count >= MOCK_RTOS_MUTEX_COUNT) {
    fail_next_mutex_create = false;
    return NULL;
  }
  return &mutexes[created_mutex_count++];
}

rtos_status_t rtos_event_flags_set(rtos_event_flags_t *events, uint32_t bits) {
  if (!events_are_valid(events) || bits == 0u) return RTOS_STATUS_INVALID_ARGUMENT;
  record_operation(MOCK_RTOS_OPERATION_EVENT_SET);
  event_set_count++;
  last_event_set_bits = bits;
  events_storage.bits |= bits;
  return RTOS_STATUS_OK;
}

rtos_status_t rtos_event_flags_wait(
  rtos_event_flags_t *events,
  uint32_t bits_to_wait,
  bool wait_all,
  bool clear_on_exit,
  uint32_t timeout_ticks,
  uint32_t *received_bits
) {
  if (!events_are_valid(events) || bits_to_wait == 0u || received_bits == NULL) {
    return RTOS_STATUS_INVALID_ARGUMENT;
  }
  record_operation(MOCK_RTOS_OPERATION_EVENT_WAIT);
  event_wait_count++;
  last_wait_bits = bits_to_wait;
  last_wait_all = wait_all;
  last_clear_on_exit = clear_on_exit;
  last_wait_timeout = timeout_ticks;
  const uint32_t available = events_storage.bits & bits_to_wait;
  if ((wait_all && available != bits_to_wait) || (!wait_all && available == 0u)) {
    return RTOS_STATUS_TIMEOUT;
  }

  *received_bits = available;
  if (clear_on_exit) events_storage.bits &= ~available;
  return RTOS_STATUS_OK;
}

rtos_status_t rtos_mutex_lock(rtos_mutex_t *mutex, uint32_t timeout_ticks) {
  if (!mutex_is_valid(mutex)) return RTOS_STATUS_INVALID_ARGUMENT;
  record_operation(lock_operation(mutex));
  if (lock_count < MOCK_RTOS_MAX_LOCKS) {
    lock_timeouts[lock_count] = timeout_ticks;
  }
  lock_count++;
  if (force_next_lock_status) {
    force_next_lock_status = false;
    return next_lock_status;
  }
  if (mutex->locked) return RTOS_STATUS_TIMEOUT;

  mutex->locked = true;
  return RTOS_STATUS_OK;
}

rtos_status_t rtos_mutex_unlock(rtos_mutex_t *mutex) {
  if (!mutex_is_valid(mutex)) return RTOS_STATUS_INVALID_ARGUMENT;
  record_operation(unlock_operation(mutex));
  unlock_count++;
  if (force_next_unlock_status && unlock_count == forced_unlock_call) {
    force_next_unlock_status = false;
    return next_unlock_status;
  }
  if (!mutex->locked) return RTOS_STATUS_NOT_OWNER;

  mutex->locked = false;
  return RTOS_STATUS_OK;
}

rtos_status_t rtos_configuration_apply(uint32_t generation) {
  (void) generation;
  record_operation(MOCK_RTOS_OPERATION_CONFIGURATION_APPLY);
  apply_count++;
  if (!force_next_apply_status) return RTOS_STATUS_OK;

  force_next_apply_status = false;
  return next_apply_status;
}

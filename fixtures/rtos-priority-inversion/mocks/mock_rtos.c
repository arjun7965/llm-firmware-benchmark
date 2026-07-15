#include "mock_rtos.h"

#include <string.h>

typedef struct {
  bool registered;
  bool blocked;
  uint32_t base_priority;
  uint32_t effective_priority;
} mock_task_state_t;

struct rtos_mutex {
  bool priority_inheritance;
  mock_rtos_task_t owner;
  bool waiters[MOCK_RTOS_TASK_COUNT];
};

static mock_task_state_t tasks[MOCK_RTOS_TASK_COUNT];
static struct rtos_mutex mutex_storage;
static mock_rtos_task_t current_task;
static bool mutex_created;
static bool fail_next_create;
static bool force_next_lock_status;
static rtos_status_t next_lock_status;
static size_t mutex_create_count;
static size_t lock_count;
static uint32_t last_lock_timeout_ticks;
static size_t unlock_count;

static bool task_is_valid(mock_rtos_task_t task) {
  return task > MOCK_RTOS_TASK_NONE &&
    task < MOCK_RTOS_TASK_COUNT &&
    tasks[task].registered;
}

static bool mutex_is_valid(const rtos_mutex_t *mutex) {
  return mutex_created && mutex == &mutex_storage;
}

static void recompute_effective_priorities(void) {
  for (size_t index = 0u; index < MOCK_RTOS_TASK_COUNT; index++) {
    tasks[index].effective_priority = tasks[index].base_priority;
  }

  if (
    !mutex_created ||
    !mutex_storage.priority_inheritance ||
    !task_is_valid(mutex_storage.owner)
  ) {
    return;
  }

  for (size_t index = 0u; index < MOCK_RTOS_TASK_COUNT; index++) {
    const mock_rtos_task_t waiter = (mock_rtos_task_t) index;
    if (
      mutex_storage.waiters[index] &&
      task_is_valid(waiter) &&
      tasks[waiter].effective_priority >
        tasks[mutex_storage.owner].effective_priority
    ) {
      tasks[mutex_storage.owner].effective_priority =
        tasks[waiter].effective_priority;
    }
  }
}

void mock_rtos_reset(void) {
  memset(tasks, 0, sizeof(tasks));
  memset(&mutex_storage, 0, sizeof(mutex_storage));
  current_task = MOCK_RTOS_TASK_NONE;
  mutex_created = false;
  fail_next_create = false;
  force_next_lock_status = false;
  next_lock_status = RTOS_STATUS_OK;
  mutex_create_count = 0u;
  lock_count = 0u;
  last_lock_timeout_ticks = 0u;
  unlock_count = 0u;
}

void mock_rtos_fail_next_mutex_create(void) {
  fail_next_create = true;
}

bool mock_rtos_register_task(mock_rtos_task_t task, uint32_t base_priority) {
  if (
    task <= MOCK_RTOS_TASK_NONE ||
    task >= MOCK_RTOS_TASK_COUNT ||
    base_priority == 0u
  ) {
    return false;
  }
  tasks[task] = (mock_task_state_t) {
    .registered = true,
    .blocked = false,
    .base_priority = base_priority,
    .effective_priority = base_priority,
  };
  recompute_effective_priorities();
  return true;
}

void mock_rtos_set_current_task(mock_rtos_task_t task) {
  current_task = task_is_valid(task) ? task : MOCK_RTOS_TASK_NONE;
}

void mock_rtos_force_next_lock_status(rtos_status_t status) {
  force_next_lock_status = true;
  next_lock_status = status;
}

size_t mock_rtos_mutex_create_count(void) {
  return mutex_create_count;
}

bool mock_rtos_last_mutex_has_priority_inheritance(void) {
  return mutex_created && mutex_storage.priority_inheritance;
}

size_t mock_rtos_lock_count(void) {
  return lock_count;
}

uint32_t mock_rtos_last_lock_timeout_ticks(void) {
  return last_lock_timeout_ticks;
}

size_t mock_rtos_unlock_count(void) {
  return unlock_count;
}

bool mock_rtos_task_is_blocked(mock_rtos_task_t task) {
  return task_is_valid(task) && tasks[task].blocked;
}

uint32_t mock_rtos_effective_priority(mock_rtos_task_t task) {
  return task_is_valid(task) ? tasks[task].effective_priority : 0u;
}

mock_rtos_task_t mock_rtos_next_runnable_task(void) {
  mock_rtos_task_t selected = MOCK_RTOS_TASK_NONE;
  uint32_t selected_priority = 0u;

  for (size_t index = 0u; index < MOCK_RTOS_TASK_COUNT; index++) {
    const mock_rtos_task_t task = (mock_rtos_task_t) index;
    if (
      task_is_valid(task) &&
      !tasks[task].blocked &&
      tasks[task].effective_priority > selected_priority
    ) {
      selected = task;
      selected_priority = tasks[task].effective_priority;
    }
  }
  return selected;
}

rtos_mutex_t *rtos_mutex_create(bool priority_inheritance) {
  mutex_create_count++;
  if (fail_next_create || mutex_created) {
    fail_next_create = false;
    return NULL;
  }
  memset(&mutex_storage, 0, sizeof(mutex_storage));
  mutex_storage.priority_inheritance = priority_inheritance;
  mutex_storage.owner = MOCK_RTOS_TASK_NONE;
  mutex_created = true;
  return &mutex_storage;
}

rtos_status_t rtos_mutex_lock(rtos_mutex_t *mutex, uint32_t timeout_ticks) {
  if (!mutex_is_valid(mutex) || !task_is_valid(current_task)) {
    return RTOS_STATUS_INVALID_ARGUMENT;
  }
  lock_count++;
  last_lock_timeout_ticks = timeout_ticks;

  if (force_next_lock_status) {
    force_next_lock_status = false;
    return next_lock_status;
  }
  if (mutex_storage.owner == MOCK_RTOS_TASK_NONE) {
    mutex_storage.owner = current_task;
    tasks[current_task].blocked = false;
    recompute_effective_priorities();
    return RTOS_STATUS_OK;
  }
  if (mutex_storage.owner == current_task) return RTOS_STATUS_ERROR;
  if (timeout_ticks == 0u) return RTOS_STATUS_TIMEOUT;

  mutex_storage.waiters[current_task] = true;
  tasks[current_task].blocked = true;
  recompute_effective_priorities();
  return RTOS_STATUS_BLOCKED;
}

rtos_status_t rtos_mutex_unlock(rtos_mutex_t *mutex) {
  if (!mutex_is_valid(mutex) || !task_is_valid(current_task)) {
    return RTOS_STATUS_INVALID_ARGUMENT;
  }
  unlock_count++;
  if (mutex_storage.owner != current_task) return RTOS_STATUS_NOT_OWNER;

  mutex_storage.owner = MOCK_RTOS_TASK_NONE;
  for (size_t index = 0u; index < MOCK_RTOS_TASK_COUNT; index++) {
    mutex_storage.waiters[index] = false;
    tasks[index].blocked = false;
  }
  recompute_effective_priorities();
  return RTOS_STATUS_OK;
}

#ifndef MOCK_RTOS_H
#define MOCK_RTOS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_rtos.h"

typedef enum {
  MOCK_RTOS_TASK_NONE = 0,
  MOCK_RTOS_TASK_TELEMETRY,
  MOCK_RTOS_TASK_DIAGNOSTICS,
  MOCK_RTOS_TASK_SAFETY,
  MOCK_RTOS_TASK_COUNT,
} mock_rtos_task_t;

void mock_rtos_reset(void);
void mock_rtos_fail_next_mutex_create(void);
bool mock_rtos_register_task(mock_rtos_task_t task, uint32_t base_priority);
void mock_rtos_set_current_task(mock_rtos_task_t task);
void mock_rtos_force_next_lock_status(rtos_status_t status);

size_t mock_rtos_mutex_create_count(void);
bool mock_rtos_last_mutex_has_priority_inheritance(void);
size_t mock_rtos_lock_count(void);
uint32_t mock_rtos_last_lock_timeout_ticks(void);
size_t mock_rtos_unlock_count(void);
bool mock_rtos_task_is_blocked(mock_rtos_task_t task);
uint32_t mock_rtos_effective_priority(mock_rtos_task_t task);
mock_rtos_task_t mock_rtos_next_runnable_task(void);

#endif

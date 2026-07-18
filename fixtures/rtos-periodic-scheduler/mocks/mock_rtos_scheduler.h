#ifndef MOCK_RTOS_SCHEDULER_H
#define MOCK_RTOS_SCHEDULER_H

#include <stddef.h>
#include <stdint.h>

#include "fixture_rtos_scheduler.h"

void mock_rtos_scheduler_reset(void);
void mock_rtos_scheduler_fail_next_release(rtos_status_t status);
void mock_rtos_scheduler_fail_release_on_call(
  size_t call_index,
  rtos_status_t status
);
size_t mock_rtos_scheduler_release_count(void);
rtos_scheduled_task_t mock_rtos_scheduler_release_task(size_t index);
uint32_t mock_rtos_scheduler_release_deadline(size_t index);

#endif

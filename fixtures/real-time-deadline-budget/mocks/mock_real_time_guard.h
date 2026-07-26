#ifndef MOCK_REAL_TIME_GUARD_H
#define MOCK_REAL_TIME_GUARD_H

#include <stddef.h>
#include <stdint.h>

#include "fixture_real_time_guard.h"

typedef enum {
  MOCK_REALTIME_EVENT_BEGIN,
  MOCK_REALTIME_EVENT_FINISH,
  MOCK_REALTIME_EVENT_VIOLATION,
  MOCK_REALTIME_EVENT_COUNT,
} mock_realtime_event_t;

void mock_realtime_reset(void);
void mock_realtime_fail_next_begin(rtos_status_t status);
void mock_realtime_fail_next_finish(rtos_status_t status);

size_t mock_realtime_event_count(void);
mock_realtime_event_t mock_realtime_event_at(size_t index);
real_time_task_t mock_realtime_event_task(size_t index);
real_time_violation_t mock_realtime_event_violation(size_t index);
uint32_t mock_realtime_event_release(size_t index);
uint32_t mock_realtime_event_time(size_t index);

#endif

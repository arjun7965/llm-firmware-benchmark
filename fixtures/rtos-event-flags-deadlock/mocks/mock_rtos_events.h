#ifndef MOCK_RTOS_EVENTS_H
#define MOCK_RTOS_EVENTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_rtos_events.h"

typedef enum {
  MOCK_RTOS_OPERATION_EVENT_SET = 0,
  MOCK_RTOS_OPERATION_EVENT_WAIT,
  MOCK_RTOS_OPERATION_CONFIGURATION_LOCK,
  MOCK_RTOS_OPERATION_ACTUATOR_LOCK,
  MOCK_RTOS_OPERATION_CONFIGURATION_APPLY,
  MOCK_RTOS_OPERATION_ACTUATOR_UNLOCK,
  MOCK_RTOS_OPERATION_CONFIGURATION_UNLOCK,
} mock_rtos_operation_t;

void mock_rtos_events_reset(void);
void mock_rtos_events_fail_next_event_create(void);
void mock_rtos_events_fail_next_mutex_create(void);
void mock_rtos_events_force_next_lock_status(rtos_status_t status);
void mock_rtos_events_force_next_unlock_status(rtos_status_t status);
void mock_rtos_events_force_unlock_status_on_call(
  size_t call_index,
  rtos_status_t status
);
void mock_rtos_events_force_next_apply_status(rtos_status_t status);
void mock_rtos_events_lock_actuator_from_peer(void);
void mock_rtos_events_unlock_actuator_from_peer(void);

size_t mock_rtos_events_create_count(void);
size_t mock_rtos_mutex_create_count(void);
size_t mock_rtos_event_set_count(void);
size_t mock_rtos_event_wait_count(void);
size_t mock_rtos_lock_count(void);
size_t mock_rtos_unlock_count(void);
size_t mock_rtos_apply_count(void);
uint32_t mock_rtos_event_bits(void);
uint32_t mock_rtos_last_event_set_bits(void);
uint32_t mock_rtos_last_wait_bits(void);
bool mock_rtos_last_wait_all(void);
bool mock_rtos_last_clear_on_exit(void);
uint32_t mock_rtos_last_wait_timeout(void);
uint32_t mock_rtos_lock_timeout(size_t index);
bool mock_rtos_configuration_locked(void);
bool mock_rtos_actuator_locked(void);
size_t mock_rtos_operation_count(void);
mock_rtos_operation_t mock_rtos_operation(size_t index);

#endif

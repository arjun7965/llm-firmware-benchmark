#ifndef MOCK_RTOS_QUEUE_H
#define MOCK_RTOS_QUEUE_H

#include <stddef.h>
#include <stdint.h>

#include "fixture_rtos_queue.h"

typedef enum {
  MOCK_RTOS_OPERATION_QUEUE_SEND = 0,
  MOCK_RTOS_OPERATION_SEMAPHORE_GIVE,
  MOCK_RTOS_OPERATION_SEMAPHORE_TAKE,
  MOCK_RTOS_OPERATION_QUEUE_RECEIVE,
} mock_rtos_operation_t;

void mock_rtos_queue_reset(void);
void mock_rtos_queue_fail_next_queue_create(void);
void mock_rtos_queue_fail_next_semaphore_create(void);
void mock_rtos_queue_force_next_send_status(rtos_status_t status);
void mock_rtos_queue_force_next_give_status(rtos_status_t status);
void mock_rtos_queue_force_next_take_status(rtos_status_t status);
void mock_rtos_queue_force_next_receive_status(rtos_status_t status);

size_t mock_rtos_queue_create_count(void);
size_t mock_rtos_semaphore_create_count(void);
size_t mock_rtos_queue_item_size(void);
size_t mock_rtos_queue_capacity(void);
uint32_t mock_rtos_semaphore_maximum_count(void);
uint32_t mock_rtos_semaphore_initial_count(void);
size_t mock_rtos_queue_send_count(void);
size_t mock_rtos_queue_receive_count(void);
size_t mock_rtos_semaphore_give_count(void);
size_t mock_rtos_semaphore_take_count(void);
uint32_t mock_rtos_last_send_timeout(void);
uint32_t mock_rtos_last_receive_timeout(void);
uint32_t mock_rtos_last_take_timeout(void);
size_t mock_rtos_queue_item_count(void);
uint32_t mock_rtos_semaphore_count(void);
size_t mock_rtos_operation_count(void);
mock_rtos_operation_t mock_rtos_operation(size_t index);

#endif

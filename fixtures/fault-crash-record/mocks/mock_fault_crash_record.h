#ifndef MOCK_FAULT_CRASH_RECORD_H
#define MOCK_FAULT_CRASH_RECORD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fault_crash_record.h"

typedef enum {
  MOCK_FAULT_EVENT_STATUS_READ,
  MOCK_FAULT_EVENT_STATUS_CLEAR_WRITE,
  MOCK_FAULT_EVENT_CONTROL_WRITE,
  MOCK_FAULT_EVENT_IRQ_SAVE_DISABLE,
  MOCK_FAULT_EVENT_IRQ_RESTORE,
  MOCK_FAULT_EVENT_COUNT,
} mock_fault_event_t;

void mock_fault0_reset(void);
void mock_fault0_watch_record(const fault_record_t *record);
fault_record_t mock_fault0_record_at(size_t index);
volatile fault0_registers_t *mock_fault0(void);
void mock_fault0_set_status(uint32_t value);
void mock_fault0_set_irq_state(uint32_t value);

uint32_t mock_fault0_status(void);
uint32_t mock_fault0_control(void);
uint32_t mock_fault0_irq_state(void);
size_t mock_fault0_event_count(void);
mock_fault_event_t mock_fault0_event_at(size_t index);
uint32_t mock_fault0_event_value(size_t index);
bool mock_fault0_invalid_access(void);

#endif

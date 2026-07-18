#ifndef MOCK_INTERRUPT_WORK_H
#define MOCK_INTERRUPT_WORK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "deferred_work.h"

typedef enum {
  MOCK_INTERRUPT_WORK_EVENT_STATUS_READ,
  MOCK_INTERRUPT_WORK_EVENT_STATUS_CLEAR,
  MOCK_INTERRUPT_WORK_EVENT_ENABLE_WRITE,
  MOCK_INTERRUPT_WORK_EVENT_IRQ_SAVE,
  MOCK_INTERRUPT_WORK_EVENT_IRQ_RESTORE,
  MOCK_INTERRUPT_WORK_EVENT_COUNT,
} mock_interrupt_work_event_t;

typedef void (*mock_interrupt_work_hook_t)(void *context);

void mock_interrupt_work_reset(void);
volatile interrupt_work_latch_t *mock_interrupt_work_latch(void);
void mock_interrupt_work_raise(uint32_t sources);
void mock_interrupt_work_set_next_status_read_hook(
  mock_interrupt_work_hook_t hook,
  void *context
);

void mock_interrupt_work_clear_events(void);
size_t mock_interrupt_work_event_count(void);
mock_interrupt_work_event_t mock_interrupt_work_event_at(size_t index);
uint32_t mock_interrupt_work_event_value(size_t index);

uint32_t mock_interrupt_work_status(void);
uint32_t mock_interrupt_work_enable(void);
void mock_interrupt_work_set_irq_state(uint32_t state);
uint32_t mock_interrupt_work_irq_state(void);
size_t mock_interrupt_work_irq_save_count(void);
size_t mock_interrupt_work_irq_restore_count(void);
uint32_t mock_interrupt_work_last_irq_restore(void);
bool mock_interrupt_work_invalid_access(void);

#endif

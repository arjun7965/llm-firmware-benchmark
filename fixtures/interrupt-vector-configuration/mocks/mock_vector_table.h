#ifndef MOCK_VECTOR_TABLE_H
#define MOCK_VECTOR_TABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "interrupt_vector.h"

typedef enum {
  MOCK_VECTOR_EVENT_NVIC_ICER,
  MOCK_VECTOR_EVENT_NVIC_ICPR,
  MOCK_VECTOR_EVENT_TABLE_WRITE,
  MOCK_VECTOR_EVENT_BARRIER,
  MOCK_VECTOR_EVENT_SCB_VTOR,
  MOCK_VECTOR_EVENT_IRQ_SAVE,
  MOCK_VECTOR_EVENT_IRQ_RESTORE,
  MOCK_VECTOR_EVENT_NVIC_ISER,
  MOCK_VECTOR_EVENT_COUNT,
} mock_vector_event_t;

void mock_vector_reset(void);
void mock_vector_clear_log(void);
volatile system_control_block_t *mock_vector_scb(void);
volatile nvic_t *mock_vector_nvic(void);

void mock_vector_set_pending_irqs(uint32_t value);
void mock_vector_set_enabled_irqs(uint32_t value);
uint32_t mock_vector_pending_irqs(void);
uint32_t mock_vector_enabled_irqs(void);
uintptr_t mock_vector_vtor(void);

void mock_vector_set_table_address_override(uintptr_t address);
void mock_vector_clear_table_address_override(void);

size_t mock_vector_event_count(void);
mock_vector_event_t mock_vector_event_at(size_t index);
size_t mock_vector_event_index(size_t index);
uintptr_t mock_vector_event_value(size_t index);

void mock_vector_set_interrupts_enabled(bool enabled);
bool mock_vector_interrupts_enabled(void);
size_t mock_vector_irq_save_count(void);
size_t mock_vector_irq_restore_count(void);
uint32_t mock_vector_last_irq_restore(void);
bool mock_vector_invalid_access(void);

#endif

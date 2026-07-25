#ifndef MOCK_IDEMPOTENT_SYSTEM_INIT_H
#define MOCK_IDEMPOTENT_SYSTEM_INIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_idempotent_system_init.h"

typedef enum {
  MOCK_SYSTEM_EVENT_CONTROL_WRITE,
  MOCK_SYSTEM_EVENT_CLOCK_WRITE,
  MOCK_SYSTEM_EVENT_PERIPHERAL_MASK_WRITE,
  MOCK_SYSTEM_EVENT_IRQ_SAVE_DISABLE,
  MOCK_SYSTEM_EVENT_IRQ_RESTORE,
  MOCK_SYSTEM_EVENT_COUNT,
} mock_system_event_t;

void mock_system0_reset(void);
volatile system0_registers_t *mock_system0(void);
void mock_system0_set_irq_state(uint32_t value);

uint32_t mock_system0_control(void);
uint32_t mock_system0_clock_hz(void);
uint32_t mock_system0_peripheral_mask(void);
uint32_t mock_system0_irq_state(void);
size_t mock_system0_event_count(void);
mock_system_event_t mock_system0_event_at(size_t index);
uint32_t mock_system0_event_value(size_t index);
bool mock_system0_invalid_access(void);

#endif

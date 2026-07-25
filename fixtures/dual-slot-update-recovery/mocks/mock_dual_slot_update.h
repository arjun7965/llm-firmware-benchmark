#ifndef MOCK_DUAL_SLOT_UPDATE_H
#define MOCK_DUAL_SLOT_UPDATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_dual_slot_update.h"

typedef enum {
  MOCK_FLASH0_EVENT_ERASE,
  MOCK_FLASH0_EVENT_PROGRAM,
  MOCK_FLASH0_EVENT_VERIFY,
  MOCK_FLASH0_EVENT_BOOT_SLOT_WRITE,
  MOCK_FLASH0_EVENT_IRQ_SAVE_DISABLE,
  MOCK_FLASH0_EVENT_IRQ_RESTORE,
  MOCK_FLASH0_EVENT_COUNT,
} mock_flash0_event_t;

void mock_flash0_reset(void);
volatile flash0_registers_t *mock_flash0(void);
void mock_flash0_set_verify_valid(update_slot_t slot, bool valid);
void mock_flash0_set_irq_state(uint32_t state);

update_slot_t mock_flash0_boot_slot(void);
size_t mock_flash0_programmed_count(update_slot_t slot);
uint32_t mock_flash0_irq_state(void);
size_t mock_flash0_event_count(void);
mock_flash0_event_t mock_flash0_event_at(size_t index);
uint32_t mock_flash0_event_value(size_t index);
bool mock_flash0_invalid_access(void);

#endif

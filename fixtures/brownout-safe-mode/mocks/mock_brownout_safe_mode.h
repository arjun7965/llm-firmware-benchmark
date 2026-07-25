#ifndef MOCK_BROWNOUT_SAFE_MODE_H
#define MOCK_BROWNOUT_SAFE_MODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_brownout_safe_mode.h"

typedef enum {
  MOCK_PWR_EVENT_STATUS_READ,
  MOCK_PWR_EVENT_SUPPLY_READ,
  MOCK_PWR_EVENT_STATUS_CLEAR_WRITE,
  MOCK_PWR_EVENT_LOAD_WRITE,
  MOCK_PWR_EVENT_IRQ_SAVE_DISABLE,
  MOCK_PWR_EVENT_IRQ_RESTORE,
  MOCK_PWR_EVENT_COUNT,
} mock_pwr_event_t;

void mock_pwr0_reset(void);
volatile pwr0_registers_t *mock_pwr0(void);
void mock_pwr0_set_status(uint32_t value);
void mock_pwr0_set_supply_mv(uint16_t value);
void mock_pwr0_set_irq_state(uint32_t value);

uint32_t mock_pwr0_status(void);
uint16_t mock_pwr0_supply_mv(void);
uint32_t mock_pwr0_load_control(void);
uint32_t mock_pwr0_irq_state(void);
size_t mock_pwr0_event_count(void);
mock_pwr_event_t mock_pwr0_event_at(size_t index);
uint32_t mock_pwr0_event_value(size_t index);
bool mock_pwr0_invalid_access(void);

#endif

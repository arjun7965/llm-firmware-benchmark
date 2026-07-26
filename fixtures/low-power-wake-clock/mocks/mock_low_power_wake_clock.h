#ifndef MOCK_LOW_POWER_WAKE_CLOCK_H
#define MOCK_LOW_POWER_WAKE_CLOCK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_low_power_wake_clock.h"

typedef enum {
  MOCK_PWRCLK_EVENT_CLOCK_WRITE,
  MOCK_PWRCLK_EVENT_WAKE_ENABLE_WRITE,
  MOCK_PWRCLK_EVENT_WAKE_STATUS_READ,
  MOCK_PWRCLK_EVENT_WAKE_CLEAR_WRITE,
  MOCK_PWRCLK_EVENT_SLEEP_MODE_WRITE,
  MOCK_PWRCLK_EVENT_IRQ_SAVE_DISABLE,
  MOCK_PWRCLK_EVENT_IRQ_RESTORE,
  MOCK_PWRCLK_EVENT_COUNT,
} mock_pwrclk_event_t;

void mock_pwrclk_reset(void);
volatile pwrclk0_registers_t *mock_pwrclk0(void);
void mock_pwrclk_set_wake_status(uint32_t value);
void mock_pwrclk_raise_wake(uint32_t value);
void mock_pwrclk_set_irq_state(uint32_t value);

pwrclk0_clock_t mock_pwrclk_clock(void);
pwrclk0_sleep_t mock_pwrclk_sleep_mode(void);
uint32_t mock_pwrclk_wake_enable(void);
uint32_t mock_pwrclk_wake_status(void);
uint32_t mock_pwrclk_irq_state(void);
size_t mock_pwrclk_event_count(void);
mock_pwrclk_event_t mock_pwrclk_event_at(size_t index);
uint32_t mock_pwrclk_event_value(size_t index);
bool mock_pwrclk_invalid_access(void);

#endif

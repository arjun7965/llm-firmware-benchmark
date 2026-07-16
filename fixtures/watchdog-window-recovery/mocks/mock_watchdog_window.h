#ifndef MOCK_WATCHDOG_WINDOW_H
#define MOCK_WATCHDOG_WINDOW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_watchdog_window.h"

typedef enum {
  MOCK_WDT_EVENT_STATUS_READ,
  MOCK_WDT_EVENT_COUNTER_READ,
  MOCK_WDT_EVENT_CONTROL_WRITE,
  MOCK_WDT_EVENT_TIMEOUT_WRITE,
  MOCK_WDT_EVENT_WINDOW_OPEN_WRITE,
  MOCK_WDT_EVENT_FEED_WRITE,
  MOCK_WDT_EVENT_STATUS_CLEAR_WRITE,
  MOCK_WDT_EVENT_IRQ_SAVE_DISABLE,
  MOCK_WDT_EVENT_IRQ_RESTORE,
  MOCK_WDT_EVENT_COUNT,
} mock_wdt_event_t;

void mock_wdt0_reset(void);
volatile wdt0_registers_t *mock_wdt0(void);
void mock_wdt0_advance(uint16_t ticks);
void mock_wdt0_trigger_reset(void);
void mock_wdt0_set_status(uint32_t value);
void mock_wdt0_set_irq_state(uint32_t value);

uint32_t mock_wdt0_status(void);
uint32_t mock_wdt0_control(void);
uint16_t mock_wdt0_timeout(void);
uint16_t mock_wdt0_window_open(void);
uint16_t mock_wdt0_counter(void);
uint32_t mock_wdt0_reset_count(void);
uint32_t mock_wdt0_irq_state(void);
size_t mock_wdt0_event_count(void);
mock_wdt_event_t mock_wdt0_event_at(size_t index);
uint32_t mock_wdt0_event_value(size_t index);
bool mock_wdt0_invalid_access(void);

#endif

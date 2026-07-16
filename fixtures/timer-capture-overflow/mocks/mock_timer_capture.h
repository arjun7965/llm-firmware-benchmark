#ifndef MOCK_TIMER_CAPTURE_H
#define MOCK_TIMER_CAPTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_timer_capture.h"

typedef enum {
  MOCK_TIMER_CAPTURE_EVENT_CONTROL_WRITE,
  MOCK_TIMER_CAPTURE_EVENT_COMPARE_WRITE,
  MOCK_TIMER_CAPTURE_EVENT_STATUS_READ,
  MOCK_TIMER_CAPTURE_EVENT_COUNT_READ,
  MOCK_TIMER_CAPTURE_EVENT_CAPTURE_READ,
  MOCK_TIMER_CAPTURE_EVENT_STATUS_CLEAR_WRITE,
  MOCK_TIMER_CAPTURE_EVENT_IRQ_SAVE_DISABLE,
  MOCK_TIMER_CAPTURE_EVENT_IRQ_RESTORE,
  MOCK_TIMER_CAPTURE_EVENT_COUNT,
} mock_timer_capture_event_t;

void mock_timer_capture_reset(void);
volatile timer1_registers_t *mock_timer1(void);
void mock_timer_capture_set_count(uint16_t value);
void mock_timer_capture_advance(uint32_t ticks);
void mock_timer_capture_latch_capture(uint16_t value);
void mock_timer_capture_latch_status(uint32_t value);
void mock_timer_capture_set_irq_state(uint32_t value);

uint32_t mock_timer1_control(void);
uint16_t mock_timer1_count(void);
uint16_t mock_timer1_capture(void);
uint16_t mock_timer1_compare(void);
uint32_t mock_timer1_status(void);
uint32_t mock_timer_capture_irq_state(void);
size_t mock_timer_capture_event_count(void);
mock_timer_capture_event_t mock_timer_capture_event_at(size_t index);
uint32_t mock_timer_capture_event_value(size_t index);
bool mock_timer_capture_invalid_access(void);

#endif

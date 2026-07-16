#ifndef TIMER_CAPTURE_H
#define TIMER_CAPTURE_H

#include <stdbool.h>
#include <stdint.h>

#include "fixture_timer_capture.h"

typedef struct {
  volatile timer1_registers_t *timer;
  uint32_t overflow_ticks;
  uint32_t capture_timestamp;
  uint32_t compare_timestamp;
  uint32_t capture_overruns;
  uint32_t compare_deadline;
  uint16_t compare_ticks;
  bool capture_pending;
  bool compare_pending;
  bool compare_armed;
  bool initialized;
} timer_capture_t;

bool timer_capture_init(
  timer_capture_t *driver,
  volatile timer1_registers_t *timer
);
bool timer_capture_arm_compare(timer_capture_t *driver, uint16_t delay_ticks);
void timer_capture_irq(timer_capture_t *driver);
bool timer_capture_take_capture(timer_capture_t *driver, uint32_t *timestamp);
bool timer_capture_take_compare(timer_capture_t *driver, uint32_t *timestamp);
uint32_t timer_capture_take_overruns(timer_capture_t *driver);

#endif

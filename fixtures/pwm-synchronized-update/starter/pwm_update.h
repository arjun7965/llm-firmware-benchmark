#ifndef PWM_UPDATE_H
#define PWM_UPDATE_H

#include <stdbool.h>
#include <stdint.h>

#include "fixture_pwm_update.h"

#define PWM_UPDATE_MAX_PERIOD_TICKS PWM0_MAX_PERIOD_TICKS

typedef enum {
  PWM_UPDATE_EVENT_NONE = 0,
  PWM_UPDATE_EVENT_APPLIED,
  PWM_UPDATE_EVENT_FAULT,
} pwm_update_event_t;

typedef struct {
  volatile pwm0_registers_t *pwm;
  uint16_t period_ticks;
  uint16_t active_duty_ticks;
  uint16_t requested_duty_ticks;
  pwm_update_event_t event;
  bool update_pending;
  bool faulted;
  bool initialized;
} pwm_update_t;

bool pwm_update_init(
  pwm_update_t *driver,
  volatile pwm0_registers_t *pwm,
  uint16_t period_ticks,
  uint16_t duty_ticks
);
bool pwm_update_request_duty(pwm_update_t *driver, uint16_t duty_ticks);
void pwm_update_irq(pwm_update_t *driver);
bool pwm_update_recover(pwm_update_t *driver);
pwm_update_event_t pwm_update_take_event(pwm_update_t *driver);

#endif

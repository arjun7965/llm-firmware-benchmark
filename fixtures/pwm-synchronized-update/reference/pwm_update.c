#include <stddef.h>

#include "pwm_update.h"

static bool pwm_update_is_ready(const pwm_update_t *driver) {
  return driver != NULL && driver->initialized && driver->pwm != NULL;
}

static void configure_pwm(
  volatile pwm0_registers_t *pwm,
  uint16_t period_ticks,
  uint16_t duty_ticks
) {
  pwm0_write_control(pwm, 0u);
  pwm0_write_period_shadow(pwm, period_ticks);
  pwm0_write_compare_shadow(pwm, duty_ticks);
  pwm0_write_status_clear(pwm, PWM0_STATUS_ALL);
  pwm0_write_load(pwm, PWM0_LOAD_ALL);
  pwm0_write_control(pwm, PWM0_CONTROL_READY);
}

bool pwm_update_init(
  pwm_update_t *driver,
  volatile pwm0_registers_t *pwm,
  uint16_t period_ticks,
  uint16_t duty_ticks
) {
  if (
    driver == NULL || pwm == NULL ||
    period_ticks == 0u || duty_ticks > period_ticks
  ) {
    return false;
  }

  configure_pwm(pwm, period_ticks, duty_ticks);
  *driver = (pwm_update_t) {
    .pwm = pwm,
    .period_ticks = period_ticks,
    .active_duty_ticks = duty_ticks,
    .requested_duty_ticks = duty_ticks,
    .event = PWM_UPDATE_EVENT_NONE,
    .update_pending = false,
    .faulted = false,
    .initialized = true,
  };
  return true;
}

bool pwm_update_request_duty(pwm_update_t *driver, uint16_t duty_ticks) {
  uint32_t irq_state;

  if (!pwm_update_is_ready(driver) || duty_ticks > driver->period_ticks) {
    return false;
  }

  irq_state = pwm0_irq_save_disable();
  if (
    driver->update_pending || driver->faulted ||
    driver->event != PWM_UPDATE_EVENT_NONE
  ) {
    pwm0_irq_restore(irq_state);
    return false;
  }

  pwm0_write_status_clear(driver->pwm, PWM0_STATUS_UPDATE);
  pwm0_write_compare_shadow(driver->pwm, duty_ticks);
  pwm0_write_load(driver->pwm, PWM0_LOAD_COMPARE);
  driver->requested_duty_ticks = duty_ticks;
  driver->update_pending = true;
  pwm0_irq_restore(irq_state);
  return true;
}

void pwm_update_irq(pwm_update_t *driver) {
  uint32_t status;

  if (!pwm_update_is_ready(driver) || driver->faulted) return;

  status = pwm0_read_status(driver->pwm) & PWM0_STATUS_ALL;
  if ((status & PWM0_STATUS_FAULT) != 0u) {
    pwm0_write_control(driver->pwm, 0u);
    pwm0_write_status_clear(driver->pwm, PWM0_STATUS_ALL);
    driver->update_pending = false;
    driver->faulted = true;
    driver->event = PWM_UPDATE_EVENT_FAULT;
    return;
  }
  if ((status & PWM0_STATUS_UPDATE) != 0u) {
    pwm0_write_status_clear(
      driver->pwm,
      PWM0_STATUS_UPDATE
    );
    if (driver->update_pending) {
      driver->active_duty_ticks = driver->requested_duty_ticks;
      driver->update_pending = false;
      driver->event = PWM_UPDATE_EVENT_APPLIED;
    }
  }
}

bool pwm_update_recover(pwm_update_t *driver) {
  uint32_t irq_state;

  if (!pwm_update_is_ready(driver)) return false;

  irq_state = pwm0_irq_save_disable();
  if (!driver->faulted) {
    pwm0_irq_restore(irq_state);
    return false;
  }
  configure_pwm(
    driver->pwm,
    driver->period_ticks,
    driver->active_duty_ticks
  );
  driver->requested_duty_ticks = driver->active_duty_ticks;
  driver->update_pending = false;
  driver->faulted = false;
  pwm0_irq_restore(irq_state);
  return true;
}

pwm_update_event_t pwm_update_take_event(pwm_update_t *driver) {
  pwm_update_event_t event;
  uint32_t irq_state;

  if (!pwm_update_is_ready(driver)) return PWM_UPDATE_EVENT_NONE;

  irq_state = pwm0_irq_save_disable();
  event = driver->event;
  driver->event = PWM_UPDATE_EVENT_NONE;
  pwm0_irq_restore(irq_state);
  return event;
}

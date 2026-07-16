#ifndef MOCK_PWM_UPDATE_H
#define MOCK_PWM_UPDATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_pwm_update.h"

typedef enum {
  MOCK_PWM_EVENT_STATUS_READ,
  MOCK_PWM_EVENT_CONTROL_WRITE,
  MOCK_PWM_EVENT_PERIOD_SHADOW_WRITE,
  MOCK_PWM_EVENT_COMPARE_SHADOW_WRITE,
  MOCK_PWM_EVENT_LOAD_WRITE,
  MOCK_PWM_EVENT_STATUS_CLEAR_WRITE,
  MOCK_PWM_EVENT_IRQ_SAVE_DISABLE,
  MOCK_PWM_EVENT_IRQ_RESTORE,
  MOCK_PWM_EVENT_COUNT,
} mock_pwm_event_t;

void mock_pwm_reset(void);
volatile pwm0_registers_t *mock_pwm0(void);
void mock_pwm_trigger_period_boundary(void);
void mock_pwm_raise_fault(void);
void mock_pwm_set_status(uint32_t value);
void mock_pwm_set_irq_state(uint32_t value);

uint32_t mock_pwm_status(void);
uint32_t mock_pwm_control(void);
uint16_t mock_pwm_period_shadow(void);
uint16_t mock_pwm_compare_shadow(void);
uint16_t mock_pwm_active_period(void);
uint16_t mock_pwm_active_duty(void);
uint32_t mock_pwm_load_pending(void);
uint32_t mock_pwm_irq_state(void);
size_t mock_pwm_event_count(void);
mock_pwm_event_t mock_pwm_event_at(size_t index);
uint32_t mock_pwm_event_value(size_t index);
bool mock_pwm_invalid_access(void);

#endif

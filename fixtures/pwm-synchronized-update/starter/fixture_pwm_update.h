#ifndef FIXTURE_PWM_UPDATE_H
#define FIXTURE_PWM_UPDATE_H

#include <stdint.h>

#define PWM0_BASE_ADDRESS UINT32_C(0x40013000)
#define PWM0_MAX_PERIOD_TICKS UINT16_MAX

#define PWM0_CONTROL_ENABLE UINT32_C(1)
#define PWM0_CONTROL_OUTPUT_ENABLE (UINT32_C(1) << 1)
#define PWM0_CONTROL_UPDATE_IRQ (UINT32_C(1) << 2)
#define PWM0_CONTROL_FAULT_IRQ (UINT32_C(1) << 3)
#define PWM0_CONTROL_READY \
  (PWM0_CONTROL_ENABLE | PWM0_CONTROL_OUTPUT_ENABLE | \
    PWM0_CONTROL_UPDATE_IRQ | PWM0_CONTROL_FAULT_IRQ)
#define PWM0_CONTROL_ALL PWM0_CONTROL_READY

#define PWM0_STATUS_UPDATE UINT32_C(1)
#define PWM0_STATUS_FAULT (UINT32_C(1) << 1)
#define PWM0_STATUS_ALL (PWM0_STATUS_UPDATE | PWM0_STATUS_FAULT)

#define PWM0_LOAD_PERIOD UINT32_C(1)
#define PWM0_LOAD_COMPARE (UINT32_C(1) << 1)
#define PWM0_LOAD_ALL (PWM0_LOAD_PERIOD | PWM0_LOAD_COMPARE)

typedef struct pwm0_registers pwm0_registers_t;

uint32_t pwm0_read_status(const volatile pwm0_registers_t *pwm);
void pwm0_write_control(volatile pwm0_registers_t *pwm, uint32_t value);
void pwm0_write_period_shadow(
  volatile pwm0_registers_t *pwm,
  uint16_t value
);
void pwm0_write_compare_shadow(
  volatile pwm0_registers_t *pwm,
  uint16_t value
);
void pwm0_write_load(volatile pwm0_registers_t *pwm, uint32_t value);
void pwm0_write_status_clear(
  volatile pwm0_registers_t *pwm,
  uint32_t value
);
uint32_t pwm0_irq_save_disable(void);
void pwm0_irq_restore(uint32_t state);

#endif

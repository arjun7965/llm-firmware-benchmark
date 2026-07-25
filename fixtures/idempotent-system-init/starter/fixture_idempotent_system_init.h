#ifndef FIXTURE_IDEMPOTENT_SYSTEM_INIT_H
#define FIXTURE_IDEMPOTENT_SYSTEM_INIT_H

#include <stdint.h>

#define SYSTEM0_MIN_CLOCK_HZ UINT32_C(8000000)
#define SYSTEM0_MAX_CLOCK_HZ UINT32_C(120000000)
#define SYSTEM0_CLOCK_STEP_HZ UINT32_C(1000000)
#define SYSTEM0_PERIPHERAL_MASK_ALL UINT32_C(0x0000000F)

#define SYSTEM0_CONTROL_SAFE UINT32_C(0)
#define SYSTEM0_CONTROL_READY UINT32_C(1)

typedef struct system0_registers system0_registers_t;

void system0_write_control(volatile system0_registers_t *system, uint32_t value);
void system0_write_clock_hz(volatile system0_registers_t *system, uint32_t value);
void system0_write_peripheral_mask(
  volatile system0_registers_t *system,
  uint32_t value
);
uint32_t system0_irq_save_disable(void);
void system0_irq_restore(uint32_t state);

#endif

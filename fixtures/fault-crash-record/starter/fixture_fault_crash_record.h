#ifndef FIXTURE_FAULT_CRASH_RECORD_H
#define FIXTURE_FAULT_CRASH_RECORD_H

#include <stdint.h>

#define FAULT0_STATUS_HARDFAULT UINT32_C(1)
#define FAULT0_STATUS_BUSFAULT (UINT32_C(1) << 1)
#define FAULT0_STATUS_ALL (FAULT0_STATUS_HARDFAULT | FAULT0_STATUS_BUSFAULT)

#define FAULT0_CONTROL_SAFE UINT32_C(0)
#define FAULT0_CONTROL_NORMAL UINT32_C(1)

typedef struct fault0_registers fault0_registers_t;

uint32_t fault0_read_status(const volatile fault0_registers_t *fault);
void fault0_write_status_clear(volatile fault0_registers_t *fault, uint32_t value);
void fault0_write_control(volatile fault0_registers_t *fault, uint32_t value);
uint32_t fault0_irq_save_disable(void);
void fault0_irq_restore(uint32_t state);

#endif

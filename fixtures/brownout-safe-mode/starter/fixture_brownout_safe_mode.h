#ifndef FIXTURE_BROWNOUT_SAFE_MODE_H
#define FIXTURE_BROWNOUT_SAFE_MODE_H

#include <stdint.h>

#define BROWNOUT_MINIMUM_MV UINT16_C(1800)
#define BROWNOUT_MAXIMUM_MV UINT16_C(3600)

#define PWR0_STATUS_BROWNOUT UINT32_C(1)
#define PWR0_STATUS_ALL PWR0_STATUS_BROWNOUT

#define PWR0_LOAD_SAFE UINT32_C(0)
#define PWR0_LOAD_ENABLED UINT32_C(1)

typedef struct pwr0_registers pwr0_registers_t;

uint32_t pwr0_read_status(const volatile pwr0_registers_t *pwr);
uint16_t pwr0_read_supply_mv(const volatile pwr0_registers_t *pwr);
void pwr0_write_status_clear(volatile pwr0_registers_t *pwr, uint32_t value);
void pwr0_write_load_control(volatile pwr0_registers_t *pwr, uint32_t value);
uint32_t pwr0_irq_save_disable(void);
void pwr0_irq_restore(uint32_t state);

#endif

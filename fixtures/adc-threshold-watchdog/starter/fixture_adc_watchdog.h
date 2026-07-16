#ifndef FIXTURE_ADC_WATCHDOG_H
#define FIXTURE_ADC_WATCHDOG_H

#include <stdint.h>

#define ADC0_BASE_ADDRESS UINT32_C(0x40012400)
#define ADC0_MAX_SAMPLE UINT16_C(4095)

#define ADC0_CONTROL_ENABLE UINT32_C(1)
#define ADC0_CONTROL_START (UINT32_C(1) << 1)
#define ADC0_CONTROL_EOC_IRQ (UINT32_C(1) << 2)
#define ADC0_CONTROL_AWD_IRQ (UINT32_C(1) << 3)
#define ADC0_CONTROL_OVERRUN_IRQ (UINT32_C(1) << 4)
#define ADC0_CONTROL_READY \
  (ADC0_CONTROL_ENABLE | ADC0_CONTROL_EOC_IRQ | ADC0_CONTROL_AWD_IRQ | \
    ADC0_CONTROL_OVERRUN_IRQ)
#define ADC0_CONTROL_ALL (ADC0_CONTROL_READY | ADC0_CONTROL_START)

#define ADC0_STATUS_EOC UINT32_C(1)
#define ADC0_STATUS_AWD (UINT32_C(1) << 1)
#define ADC0_STATUS_OVERRUN (UINT32_C(1) << 2)
#define ADC0_STATUS_ALL \
  (ADC0_STATUS_EOC | ADC0_STATUS_AWD | ADC0_STATUS_OVERRUN)

typedef struct adc0_registers adc0_registers_t;

uint32_t adc0_read_status(const volatile adc0_registers_t *adc);
uint16_t adc0_read_data(const volatile adc0_registers_t *adc);
void adc0_write_control(volatile adc0_registers_t *adc, uint32_t value);
void adc0_write_lower_threshold(
  volatile adc0_registers_t *adc,
  uint16_t value
);
void adc0_write_upper_threshold(
  volatile adc0_registers_t *adc,
  uint16_t value
);
void adc0_write_status_clear(
  volatile adc0_registers_t *adc,
  uint32_t value
);
uint32_t adc0_irq_save_disable(void);
void adc0_irq_restore(uint32_t state);

#endif

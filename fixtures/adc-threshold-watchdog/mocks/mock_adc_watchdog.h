#ifndef MOCK_ADC_WATCHDOG_H
#define MOCK_ADC_WATCHDOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_adc_watchdog.h"

typedef enum {
  MOCK_ADC_EVENT_STATUS_READ,
  MOCK_ADC_EVENT_DATA_READ,
  MOCK_ADC_EVENT_CONTROL_WRITE,
  MOCK_ADC_EVENT_LOWER_THRESHOLD_WRITE,
  MOCK_ADC_EVENT_UPPER_THRESHOLD_WRITE,
  MOCK_ADC_EVENT_STATUS_CLEAR_WRITE,
  MOCK_ADC_EVENT_IRQ_SAVE_DISABLE,
  MOCK_ADC_EVENT_IRQ_RESTORE,
  MOCK_ADC_EVENT_COUNT,
} mock_adc_event_t;

void mock_adc_reset(void);
volatile adc0_registers_t *mock_adc0(void);
void mock_adc_set_sample(uint16_t sample);
void mock_adc_set_status(uint32_t value);
void mock_adc_complete_sample(uint16_t sample);
void mock_adc_raise_overrun(void);
void mock_adc_set_irq_state(uint32_t value);

uint32_t mock_adc_status(void);
uint16_t mock_adc_data(void);
uint32_t mock_adc_control(void);
uint16_t mock_adc_lower_threshold(void);
uint16_t mock_adc_upper_threshold(void);
uint32_t mock_adc_irq_state(void);
size_t mock_adc_event_count(void);
mock_adc_event_t mock_adc_event_at(size_t index);
uint32_t mock_adc_event_value(size_t index);
bool mock_adc_invalid_access(void);

#endif

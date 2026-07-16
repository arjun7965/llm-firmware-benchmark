#ifndef ADC_WATCHDOG_H
#define ADC_WATCHDOG_H

#include <stdbool.h>
#include <stdint.h>

#include "fixture_adc_watchdog.h"

#define ADC_WATCHDOG_TIMEOUT_MS UINT32_C(25)
#define ADC_WATCHDOG_MAX_SAMPLE ADC0_MAX_SAMPLE

typedef enum {
  ADC_WATCHDOG_EVENT_NONE = 0,
  ADC_WATCHDOG_EVENT_IN_WINDOW,
  ADC_WATCHDOG_EVENT_OUT_OF_WINDOW,
  ADC_WATCHDOG_EVENT_FAULT_OVERRUN,
  ADC_WATCHDOG_EVENT_FAULT_TIMEOUT,
} adc_watchdog_event_t;

typedef struct {
  volatile adc0_registers_t *adc;
  uint16_t lower_threshold;
  uint16_t upper_threshold;
  uint16_t last_sample;
  uint32_t started_at_ms;
  adc_watchdog_event_t event;
  bool active;
  bool faulted;
  bool initialized;
} adc_watchdog_t;

bool adc_watchdog_init(
  adc_watchdog_t *driver,
  volatile adc0_registers_t *adc,
  uint16_t lower_threshold,
  uint16_t upper_threshold
);
bool adc_watchdog_start(adc_watchdog_t *driver, uint32_t now_ms);
void adc_watchdog_irq(adc_watchdog_t *driver);
adc_watchdog_event_t adc_watchdog_poll(
  adc_watchdog_t *driver,
  uint32_t now_ms
);
bool adc_watchdog_recover(adc_watchdog_t *driver);
adc_watchdog_event_t adc_watchdog_take_event(adc_watchdog_t *driver);

#endif

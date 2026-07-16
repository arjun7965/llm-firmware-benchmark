#include <stddef.h>

#include "adc_watchdog.h"

static bool adc_watchdog_is_ready(const adc_watchdog_t *driver) {
  return driver != NULL && driver->initialized && driver->adc != NULL;
}

static void configure_adc(
  volatile adc0_registers_t *adc,
  uint16_t lower_threshold,
  uint16_t upper_threshold
) {
  adc0_write_control(adc, 0u);
  adc0_write_lower_threshold(adc, lower_threshold);
  adc0_write_upper_threshold(adc, upper_threshold);
  adc0_write_status_clear(adc, ADC0_STATUS_ALL);
  adc0_write_control(adc, ADC0_CONTROL_READY);
}

bool adc_watchdog_init(
  adc_watchdog_t *driver,
  volatile adc0_registers_t *adc,
  uint16_t lower_threshold,
  uint16_t upper_threshold
) {
  if (
    driver == NULL || adc == NULL ||
    lower_threshold > upper_threshold ||
    upper_threshold > ADC_WATCHDOG_MAX_SAMPLE
  ) {
    return false;
  }

  configure_adc(adc, lower_threshold, upper_threshold);
  *driver = (adc_watchdog_t) {
    .adc = adc,
    .lower_threshold = lower_threshold,
    .upper_threshold = upper_threshold,
    .last_sample = 0u,
    .started_at_ms = 0u,
    .event = ADC_WATCHDOG_EVENT_NONE,
    .active = false,
    .faulted = false,
    .initialized = true,
  };
  return true;
}

bool adc_watchdog_start(adc_watchdog_t *driver, uint32_t now_ms) {
  uint32_t irq_state;

  if (!adc_watchdog_is_ready(driver)) return false;

  irq_state = adc0_irq_save_disable();
  if (
    driver->active || driver->faulted ||
    driver->event != ADC_WATCHDOG_EVENT_NONE
  ) {
    adc0_irq_restore(irq_state);
    return false;
  }

  driver->started_at_ms = now_ms;
  driver->active = true;
  adc0_write_status_clear(driver->adc, ADC0_STATUS_ALL);
  adc0_write_control(
    driver->adc,
    ADC0_CONTROL_READY | ADC0_CONTROL_START
  );
  adc0_irq_restore(irq_state);
  return true;
}

void adc_watchdog_irq(adc_watchdog_t *driver) {
  uint32_t status;
  uint16_t sample;

  if (!adc_watchdog_is_ready(driver) || !driver->active) return;

  status = adc0_read_status(driver->adc) & ADC0_STATUS_ALL;
  if ((status & ADC0_STATUS_OVERRUN) != 0u) {
    adc0_write_control(driver->adc, 0u);
    adc0_write_status_clear(driver->adc, ADC0_STATUS_ALL);
    driver->active = false;
    driver->faulted = true;
    driver->event = ADC_WATCHDOG_EVENT_FAULT_OVERRUN;
    return;
  }
  if ((status & (ADC0_STATUS_EOC | ADC0_STATUS_AWD)) == 0u) return;

  sample = adc0_read_data(driver->adc);
  adc0_write_status_clear(
    driver->adc,
    status & (ADC0_STATUS_EOC | ADC0_STATUS_AWD)
  );
  driver->last_sample = sample;
  driver->active = false;
  driver->event = (status & ADC0_STATUS_AWD) != 0u
    ? ADC_WATCHDOG_EVENT_OUT_OF_WINDOW
    : ADC_WATCHDOG_EVENT_IN_WINDOW;
}

adc_watchdog_event_t adc_watchdog_poll(
  adc_watchdog_t *driver,
  uint32_t now_ms
) {
  adc_watchdog_event_t event = ADC_WATCHDOG_EVENT_NONE;
  uint32_t irq_state;

  if (!adc_watchdog_is_ready(driver)) return ADC_WATCHDOG_EVENT_NONE;

  irq_state = adc0_irq_save_disable();
  if (
    driver->active &&
    (uint32_t)(now_ms - driver->started_at_ms) >=
      ADC_WATCHDOG_TIMEOUT_MS
  ) {
    adc0_write_control(driver->adc, 0u);
    adc0_write_status_clear(driver->adc, ADC0_STATUS_ALL);
    driver->active = false;
    driver->faulted = true;
    driver->event = ADC_WATCHDOG_EVENT_FAULT_TIMEOUT;
    event = driver->event;
  }
  adc0_irq_restore(irq_state);
  return event;
}

bool adc_watchdog_recover(adc_watchdog_t *driver) {
  uint32_t irq_state;

  if (!adc_watchdog_is_ready(driver)) return false;

  irq_state = adc0_irq_save_disable();
  if (!driver->faulted) {
    adc0_irq_restore(irq_state);
    return false;
  }
  configure_adc(
    driver->adc,
    driver->lower_threshold,
    driver->upper_threshold
  );
  driver->active = false;
  driver->faulted = false;
  adc0_irq_restore(irq_state);
  return true;
}

adc_watchdog_event_t adc_watchdog_take_event(adc_watchdog_t *driver) {
  adc_watchdog_event_t event;
  uint32_t irq_state;

  if (!adc_watchdog_is_ready(driver)) return ADC_WATCHDOG_EVENT_NONE;

  irq_state = adc0_irq_save_disable();
  event = driver->event;
  driver->event = ADC_WATCHDOG_EVENT_NONE;
  adc0_irq_restore(irq_state);
  return event;
}

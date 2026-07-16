#include <stddef.h>

#include "watchdog_window.h"

static bool watchdog_window_is_ready(const watchdog_window_t *driver) {
  return driver != NULL && driver->initialized && driver->wdt != NULL;
}

static void configure_watchdog(
  volatile wdt0_registers_t *wdt,
  uint16_t timeout_ticks,
  uint16_t window_open_ticks
) {
  wdt0_write_control(wdt, 0u);
  wdt0_write_timeout(wdt, timeout_ticks);
  wdt0_write_window_open(wdt, window_open_ticks);
  wdt0_write_status_clear(wdt, WDT0_STATUS_RESET);
  wdt0_write_control(wdt, WDT0_CONTROL_READY);
}

bool watchdog_window_init(
  watchdog_window_t *driver,
  volatile wdt0_registers_t *wdt,
  uint16_t timeout_ticks,
  uint16_t window_open_ticks
) {
  bool reset_recovered;

  if (
    driver == NULL || wdt == NULL || timeout_ticks < UINT16_C(2) ||
    timeout_ticks > WATCHDOG_WINDOW_MAX_TIMEOUT_TICKS ||
    window_open_ticks == 0u || window_open_ticks >= timeout_ticks
  ) {
    return false;
  }

  reset_recovered = (wdt0_read_status(wdt) & WDT0_STATUS_RESET) != 0u;
  configure_watchdog(wdt, timeout_ticks, window_open_ticks);
  *driver = (watchdog_window_t) {
    .wdt = wdt,
    .timeout_ticks = timeout_ticks,
    .window_open_ticks = window_open_ticks,
    .event = reset_recovered
      ? WATCHDOG_WINDOW_EVENT_RESET_RECOVERED
      : WATCHDOG_WINDOW_EVENT_NONE,
    .reset_pending = false,
    .initialized = true,
  };
  return true;
}

watchdog_window_feed_result_t watchdog_window_feed(
  watchdog_window_t *driver
) {
  uint16_t counter_ticks;
  uint32_t irq_state;
  uint32_t status;

  if (!watchdog_window_is_ready(driver)) {
    return WATCHDOG_WINDOW_FEED_INVALID;
  }

  irq_state = wdt0_irq_save_disable();
  status = wdt0_read_status(driver->wdt) & WDT0_STATUS_RESET;
  if (status != 0u) {
    driver->reset_pending = true;
    driver->event = WATCHDOG_WINDOW_EVENT_RESET_DETECTED;
    wdt0_irq_restore(irq_state);
    return WATCHDOG_WINDOW_FEED_RESET_DETECTED;
  }
  if (
    driver->reset_pending ||
    driver->event != WATCHDOG_WINDOW_EVENT_NONE
  ) {
    wdt0_irq_restore(irq_state);
    return WATCHDOG_WINDOW_FEED_INVALID;
  }

  counter_ticks = wdt0_read_counter(driver->wdt);
  if (counter_ticks < driver->window_open_ticks) {
    wdt0_irq_restore(irq_state);
    return WATCHDOG_WINDOW_FEED_TOO_EARLY;
  }

  wdt0_write_feed(driver->wdt, WDT0_FEED_KEY);
  wdt0_irq_restore(irq_state);
  return WATCHDOG_WINDOW_FEED_OK;
}

watchdog_window_event_t watchdog_window_poll(watchdog_window_t *driver) {
  uint32_t irq_state;
  uint32_t status;

  if (!watchdog_window_is_ready(driver)) {
    return WATCHDOG_WINDOW_EVENT_NONE;
  }

  irq_state = wdt0_irq_save_disable();
  status = wdt0_read_status(driver->wdt) & WDT0_STATUS_RESET;
  if (status != 0u) {
    driver->reset_pending = true;
    driver->event = WATCHDOG_WINDOW_EVENT_RESET_DETECTED;
  }
  wdt0_irq_restore(irq_state);
  return status != 0u
    ? WATCHDOG_WINDOW_EVENT_RESET_DETECTED
    : WATCHDOG_WINDOW_EVENT_NONE;
}

bool watchdog_window_recover(watchdog_window_t *driver) {
  uint32_t irq_state;
  uint32_t status;

  if (!watchdog_window_is_ready(driver)) return false;

  irq_state = wdt0_irq_save_disable();
  if (
    !driver->reset_pending ||
    driver->event != WATCHDOG_WINDOW_EVENT_NONE
  ) {
    wdt0_irq_restore(irq_state);
    return false;
  }
  status = wdt0_read_status(driver->wdt) & WDT0_STATUS_RESET;
  if (status == 0u) {
    wdt0_irq_restore(irq_state);
    return false;
  }

  configure_watchdog(
    driver->wdt,
    driver->timeout_ticks,
    driver->window_open_ticks
  );
  driver->reset_pending = false;
  driver->event = WATCHDOG_WINDOW_EVENT_RESET_RECOVERED;
  wdt0_irq_restore(irq_state);
  return true;
}

watchdog_window_event_t watchdog_window_take_event(
  watchdog_window_t *driver
) {
  watchdog_window_event_t event;
  uint32_t irq_state;

  if (!watchdog_window_is_ready(driver)) {
    return WATCHDOG_WINDOW_EVENT_NONE;
  }

  irq_state = wdt0_irq_save_disable();
  event = driver->event;
  driver->event = WATCHDOG_WINDOW_EVENT_NONE;
  wdt0_irq_restore(irq_state);
  return event;
}

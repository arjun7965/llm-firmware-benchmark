#ifndef WATCHDOG_WINDOW_H
#define WATCHDOG_WINDOW_H

#include <stdbool.h>
#include <stdint.h>

#include "fixture_watchdog_window.h"

#define WATCHDOG_WINDOW_MAX_TIMEOUT_TICKS WDT0_MAX_TIMEOUT_TICKS

typedef enum {
  WATCHDOG_WINDOW_EVENT_NONE = 0,
  WATCHDOG_WINDOW_EVENT_RESET_DETECTED,
  WATCHDOG_WINDOW_EVENT_RESET_RECOVERED,
} watchdog_window_event_t;

typedef enum {
  WATCHDOG_WINDOW_FEED_INVALID = 0,
  WATCHDOG_WINDOW_FEED_TOO_EARLY,
  WATCHDOG_WINDOW_FEED_OK,
  WATCHDOG_WINDOW_FEED_RESET_DETECTED,
} watchdog_window_feed_result_t;

typedef struct {
  volatile wdt0_registers_t *wdt;
  uint16_t timeout_ticks;
  uint16_t window_open_ticks;
  watchdog_window_event_t event;
  bool reset_pending;
  bool initialized;
} watchdog_window_t;

bool watchdog_window_init(
  watchdog_window_t *driver,
  volatile wdt0_registers_t *wdt,
  uint16_t timeout_ticks,
  uint16_t window_open_ticks
);
watchdog_window_feed_result_t watchdog_window_feed(
  watchdog_window_t *driver
);
watchdog_window_event_t watchdog_window_poll(watchdog_window_t *driver);
bool watchdog_window_recover(watchdog_window_t *driver);
watchdog_window_event_t watchdog_window_take_event(
  watchdog_window_t *driver
);

#endif

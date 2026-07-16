#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "mock_watchdog_window.h"
#include "watchdog_window.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
              __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

typedef struct {
  mock_wdt_event_t event;
  uint32_t value;
} expected_event_t;

static bool initialize(
  watchdog_window_t *driver,
  uint16_t timeout_ticks,
  uint16_t window_open_ticks
) {
  return watchdog_window_init(
    driver,
    mock_wdt0(),
    timeout_ticks,
    window_open_ticks
  );
}

static bool state_equals(
  const watchdog_window_t *left,
  const watchdog_window_t *right
) {
  return left->wdt == right->wdt &&
    left->timeout_ticks == right->timeout_ticks &&
    left->window_open_ticks == right->window_open_ticks &&
    left->event == right->event &&
    left->reset_pending == right->reset_pending &&
    left->initialized == right->initialized;
}

static bool events_match_from(
  size_t offset,
  const expected_event_t *expected,
  size_t expected_count
) {
  if (mock_wdt0_event_count() != offset + expected_count) return false;
  for (size_t index = 0u; index < expected_count; index++) {
    if (
      mock_wdt0_event_at(offset + index) != expected[index].event ||
      mock_wdt0_event_value(offset + index) != expected[index].value
    ) {
      return false;
    }
  }
  return true;
}

static bool test_initialization_validation_and_boot_recovery(void) {
  watchdog_window_t driver = {
    .wdt = (volatile wdt0_registers_t *)(uintptr_t)UINT32_C(1),
    .timeout_ticks = UINT16_C(2),
    .window_open_ticks = UINT16_C(1),
    .event = WATCHDOG_WINDOW_EVENT_RESET_DETECTED,
    .reset_pending = true,
    .initialized = true,
  };
  const watchdog_window_t before = driver;
  const expected_event_t expected[] = {
    { MOCK_WDT_EVENT_STATUS_READ, WDT0_STATUS_RESET },
    { MOCK_WDT_EVENT_CONTROL_WRITE, 0u },
    { MOCK_WDT_EVENT_TIMEOUT_WRITE, UINT16_C(20) },
    { MOCK_WDT_EVENT_WINDOW_OPEN_WRITE, UINT16_C(5) },
    { MOCK_WDT_EVENT_STATUS_CLEAR_WRITE, WDT0_STATUS_RESET },
    { MOCK_WDT_EVENT_CONTROL_WRITE, WDT0_CONTROL_READY },
  };

  mock_wdt0_reset();
  mock_wdt0_set_status(WDT0_STATUS_RESET);
  CHECK(!watchdog_window_init(NULL, mock_wdt0(), UINT16_C(20), UINT16_C(5)));
  CHECK(!watchdog_window_init(&driver, NULL, UINT16_C(20), UINT16_C(5)));
  CHECK(!watchdog_window_init(&driver, mock_wdt0(), UINT16_C(1), 0u));
  CHECK(!watchdog_window_init(&driver, mock_wdt0(), UINT16_C(20), 0u));
  CHECK(!watchdog_window_init(
    &driver,
    mock_wdt0(),
    UINT16_C(20),
    UINT16_C(20)
  ));
  CHECK(!watchdog_window_init(
    &driver,
    mock_wdt0(),
    (uint16_t)(WATCHDOG_WINDOW_MAX_TIMEOUT_TICKS + UINT16_C(1)),
    UINT16_C(1)
  ));
  CHECK(state_equals(&driver, &before));
  CHECK(mock_wdt0_event_count() == 0u);

  CHECK(initialize(&driver, UINT16_C(20), UINT16_C(5)));
  CHECK(events_match_from(0u, expected, sizeof(expected) / sizeof(expected[0])));
  CHECK(driver.wdt == mock_wdt0());
  CHECK(driver.timeout_ticks == UINT16_C(20));
  CHECK(driver.window_open_ticks == UINT16_C(5));
  CHECK(driver.event == WATCHDOG_WINDOW_EVENT_RESET_RECOVERED);
  CHECK(!driver.reset_pending);
  CHECK(driver.initialized);
  CHECK(mock_wdt0_status() == 0u);
  CHECK(mock_wdt0_control() == WDT0_CONTROL_READY);
  CHECK(mock_wdt0_timeout() == UINT16_C(20));
  CHECK(mock_wdt0_window_open() == UINT16_C(5));
  CHECK(mock_wdt0_counter() == 0u);
  CHECK(!mock_wdt0_invalid_access());
  return true;
}

static bool test_feed_window_boundaries_and_interrupt_restore(void) {
  watchdog_window_t driver = { 0 };
  const expected_event_t healthy_recovery_events[] = {
    { MOCK_WDT_EVENT_IRQ_SAVE_DISABLE, UINT32_C(1) },
    { MOCK_WDT_EVENT_IRQ_RESTORE, UINT32_C(1) },
  };
  const expected_event_t early_events[] = {
    { MOCK_WDT_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xA5) },
    { MOCK_WDT_EVENT_STATUS_READ, 0u },
    { MOCK_WDT_EVENT_COUNTER_READ, UINT16_C(4) },
    { MOCK_WDT_EVENT_IRQ_RESTORE, UINT32_C(0xA5) },
  };
  const expected_event_t feed_events[] = {
    { MOCK_WDT_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xA5) },
    { MOCK_WDT_EVENT_STATUS_READ, 0u },
    { MOCK_WDT_EVENT_COUNTER_READ, UINT16_C(5) },
    { MOCK_WDT_EVENT_FEED_WRITE, WDT0_FEED_KEY },
    { MOCK_WDT_EVENT_IRQ_RESTORE, UINT32_C(0xA5) },
  };
  size_t offset;

  mock_wdt0_reset();
  CHECK(initialize(&driver, UINT16_C(20), UINT16_C(5)));
  offset = mock_wdt0_event_count();
  CHECK(!watchdog_window_recover(&driver));
  CHECK(events_match_from(
    offset,
    healthy_recovery_events,
    sizeof(healthy_recovery_events) / sizeof(healthy_recovery_events[0])
  ));
  mock_wdt0_set_irq_state(UINT32_C(0xA5));
  mock_wdt0_advance(UINT16_C(4));
  offset = mock_wdt0_event_count();
  CHECK(watchdog_window_feed(&driver) == WATCHDOG_WINDOW_FEED_TOO_EARLY);
  CHECK(events_match_from(
    offset,
    early_events,
    sizeof(early_events) / sizeof(early_events[0])
  ));
  CHECK(mock_wdt0_counter() == UINT16_C(4));
  CHECK(mock_wdt0_reset_count() == 0u);
  CHECK(mock_wdt0_irq_state() == UINT32_C(0xA5));

  mock_wdt0_advance(UINT16_C(1));
  offset = mock_wdt0_event_count();
  CHECK(watchdog_window_feed(&driver) == WATCHDOG_WINDOW_FEED_OK);
  CHECK(events_match_from(
    offset,
    feed_events,
    sizeof(feed_events) / sizeof(feed_events[0])
  ));
  CHECK(mock_wdt0_counter() == 0u);
  CHECK(mock_wdt0_control() == WDT0_CONTROL_READY);
  CHECK(!mock_wdt0_invalid_access());
  return true;
}

static bool test_reset_detection_event_gating_and_recovery(void) {
  watchdog_window_t driver = { 0 };
  const expected_event_t detect_events[] = {
    { MOCK_WDT_EVENT_IRQ_SAVE_DISABLE, UINT32_C(1) },
    { MOCK_WDT_EVENT_STATUS_READ, WDT0_STATUS_RESET },
    { MOCK_WDT_EVENT_IRQ_RESTORE, UINT32_C(1) },
  };
  const expected_event_t rejected_recovery_events[] = {
    { MOCK_WDT_EVENT_IRQ_SAVE_DISABLE, UINT32_C(1) },
    { MOCK_WDT_EVENT_IRQ_RESTORE, UINT32_C(1) },
  };
  const expected_event_t recovery_events[] = {
    { MOCK_WDT_EVENT_IRQ_SAVE_DISABLE, UINT32_C(1) },
    { MOCK_WDT_EVENT_STATUS_READ, WDT0_STATUS_RESET },
    { MOCK_WDT_EVENT_CONTROL_WRITE, 0u },
    { MOCK_WDT_EVENT_TIMEOUT_WRITE, UINT16_C(20) },
    { MOCK_WDT_EVENT_WINDOW_OPEN_WRITE, UINT16_C(5) },
    { MOCK_WDT_EVENT_STATUS_CLEAR_WRITE, WDT0_STATUS_RESET },
    { MOCK_WDT_EVENT_CONTROL_WRITE, WDT0_CONTROL_READY },
    { MOCK_WDT_EVENT_IRQ_RESTORE, UINT32_C(1) },
  };
  const expected_event_t missing_latch_events[] = {
    { MOCK_WDT_EVENT_IRQ_SAVE_DISABLE, UINT32_C(1) },
    { MOCK_WDT_EVENT_STATUS_READ, 0u },
    { MOCK_WDT_EVENT_IRQ_RESTORE, UINT32_C(1) },
  };
  const expected_event_t gated_feed_events[] = {
    { MOCK_WDT_EVENT_IRQ_SAVE_DISABLE, UINT32_C(1) },
    { MOCK_WDT_EVENT_STATUS_READ, 0u },
    { MOCK_WDT_EVENT_IRQ_RESTORE, UINT32_C(1) },
  };
  size_t offset;

  mock_wdt0_reset();
  CHECK(initialize(&driver, UINT16_C(20), UINT16_C(5)));
  mock_wdt0_advance(UINT16_C(20));
  CHECK(mock_wdt0_status() == WDT0_STATUS_RESET);
  CHECK(mock_wdt0_control() == 0u);
  CHECK(mock_wdt0_reset_count() == UINT32_C(1));

  offset = mock_wdt0_event_count();
  CHECK(watchdog_window_feed(&driver) ==
    WATCHDOG_WINDOW_FEED_RESET_DETECTED);
  CHECK(events_match_from(
    offset,
    detect_events,
    sizeof(detect_events) / sizeof(detect_events[0])
  ));
  CHECK(driver.reset_pending);
  CHECK(driver.event == WATCHDOG_WINDOW_EVENT_RESET_DETECTED);

  offset = mock_wdt0_event_count();
  CHECK(!watchdog_window_recover(&driver));
  CHECK(events_match_from(
    offset,
    rejected_recovery_events,
    sizeof(rejected_recovery_events) / sizeof(rejected_recovery_events[0])
  ));
  CHECK(watchdog_window_take_event(&driver) ==
    WATCHDOG_WINDOW_EVENT_RESET_DETECTED);

  mock_wdt0_set_status(0u);
  offset = mock_wdt0_event_count();
  CHECK(!watchdog_window_recover(&driver));
  CHECK(events_match_from(
    offset,
    missing_latch_events,
    sizeof(missing_latch_events) / sizeof(missing_latch_events[0])
  ));
  CHECK(driver.reset_pending);
  mock_wdt0_set_status(WDT0_STATUS_RESET);

  offset = mock_wdt0_event_count();
  CHECK(watchdog_window_recover(&driver));
  CHECK(events_match_from(
    offset,
    recovery_events,
    sizeof(recovery_events) / sizeof(recovery_events[0])
  ));
  CHECK(!driver.reset_pending);
  CHECK(driver.event == WATCHDOG_WINDOW_EVENT_RESET_RECOVERED);
  CHECK(mock_wdt0_status() == 0u);
  CHECK(mock_wdt0_counter() == 0u);

  offset = mock_wdt0_event_count();
  CHECK(watchdog_window_feed(&driver) == WATCHDOG_WINDOW_FEED_INVALID);
  CHECK(events_match_from(
    offset,
    gated_feed_events,
    sizeof(gated_feed_events) / sizeof(gated_feed_events[0])
  ));
  CHECK(watchdog_window_take_event(&driver) ==
    WATCHDOG_WINDOW_EVENT_RESET_RECOVERED);
  mock_wdt0_advance(UINT16_C(5));
  CHECK(watchdog_window_feed(&driver) == WATCHDOG_WINDOW_FEED_OK);
  CHECK(!mock_wdt0_invalid_access());
  return true;
}

static bool test_poll_and_reinitialization_after_reset(void) {
  watchdog_window_t driver = { 0 };
  size_t offset;
  const expected_event_t poll_events[] = {
    { MOCK_WDT_EVENT_IRQ_SAVE_DISABLE, UINT32_C(1) },
    { MOCK_WDT_EVENT_STATUS_READ, 0u },
    { MOCK_WDT_EVENT_IRQ_RESTORE, UINT32_C(1) },
  };

  mock_wdt0_reset();
  CHECK(initialize(&driver, UINT16_C(10), UINT16_C(2)));
  mock_wdt0_advance(UINT16_C(9));
  offset = mock_wdt0_event_count();
  CHECK(watchdog_window_poll(&driver) == WATCHDOG_WINDOW_EVENT_NONE);
  CHECK(events_match_from(
    offset,
    poll_events,
    sizeof(poll_events) / sizeof(poll_events[0])
  ));
  CHECK(!driver.reset_pending);

  mock_wdt0_advance(UINT16_C(1));
  CHECK(watchdog_window_poll(&driver) ==
    WATCHDOG_WINDOW_EVENT_RESET_DETECTED);
  CHECK(driver.reset_pending);
  CHECK(watchdog_window_take_event(&driver) ==
    WATCHDOG_WINDOW_EVENT_RESET_DETECTED);
  CHECK(initialize(&driver, UINT16_C(12), UINT16_C(3)));
  CHECK(driver.timeout_ticks == UINT16_C(12));
  CHECK(driver.window_open_ticks == UINT16_C(3));
  CHECK(driver.event == WATCHDOG_WINDOW_EVENT_RESET_RECOVERED);
  CHECK(!driver.reset_pending);
  CHECK(watchdog_window_take_event(&driver) ==
    WATCHDOG_WINDOW_EVENT_RESET_RECOVERED);
  mock_wdt0_advance(UINT16_C(3));
  CHECK(watchdog_window_feed(&driver) == WATCHDOG_WINDOW_FEED_OK);
  CHECK(mock_wdt0_reset_count() == UINT32_C(1));
  CHECK(!mock_wdt0_invalid_access());
  return true;
}

static bool test_invalid_foreground_calls_have_no_side_effects(void) {
  watchdog_window_t driver = { 0 };
  const watchdog_window_t before = driver;

  mock_wdt0_reset();
  CHECK(watchdog_window_feed(NULL) == WATCHDOG_WINDOW_FEED_INVALID);
  CHECK(watchdog_window_feed(&driver) == WATCHDOG_WINDOW_FEED_INVALID);
  CHECK(watchdog_window_poll(NULL) == WATCHDOG_WINDOW_EVENT_NONE);
  CHECK(watchdog_window_poll(&driver) == WATCHDOG_WINDOW_EVENT_NONE);
  CHECK(!watchdog_window_recover(NULL));
  CHECK(!watchdog_window_recover(&driver));
  CHECK(watchdog_window_take_event(NULL) == WATCHDOG_WINDOW_EVENT_NONE);
  CHECK(watchdog_window_take_event(&driver) == WATCHDOG_WINDOW_EVENT_NONE);
  CHECK(state_equals(&driver, &before));
  CHECK(mock_wdt0_event_count() == 0u);
  CHECK(mock_wdt0_irq_state() == UINT32_C(1));
  CHECK(!mock_wdt0_invalid_access());
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "initialization and boot recovery", test_initialization_validation_and_boot_recovery },
    { "feed-window boundaries", test_feed_window_boundaries_and_interrupt_restore },
    { "reset recovery and event gating", test_reset_detection_event_gating_and_recovery },
    { "poll and reinitialization", test_poll_and_reinitialization_after_reset },
    { "invalid foreground calls", test_invalid_foreground_calls_have_no_side_effects },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) {
      fprintf(stderr, "failed: %s\n", tests[index].name);
      return 1;
    }
  }

  printf("Watchdog-window recovery public tests passed (%zu tests).\n",
    sizeof(tests) / sizeof(tests[0]));
  return 0;
}

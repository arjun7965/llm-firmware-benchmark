#ifndef FIXTURE_WATCHDOG_WINDOW_H
#define FIXTURE_WATCHDOG_WINDOW_H

#include <stdint.h>

#define WDT0_BASE_ADDRESS UINT32_C(0x40014000)
#define WDT0_MAX_TIMEOUT_TICKS UINT16_C(1024)

#define WDT0_CONTROL_ENABLE UINT32_C(1)
#define WDT0_CONTROL_RESET_ENABLE (UINT32_C(1) << 1)
#define WDT0_CONTROL_WINDOW_ENABLE (UINT32_C(1) << 2)
#define WDT0_CONTROL_READY \
  (WDT0_CONTROL_ENABLE | WDT0_CONTROL_RESET_ENABLE | \
    WDT0_CONTROL_WINDOW_ENABLE)
#define WDT0_CONTROL_ALL WDT0_CONTROL_READY

#define WDT0_STATUS_RESET UINT32_C(1)
#define WDT0_STATUS_ALL WDT0_STATUS_RESET

#define WDT0_FEED_KEY UINT32_C(0x0000A5A5)

typedef struct wdt0_registers wdt0_registers_t;

uint32_t wdt0_read_status(const volatile wdt0_registers_t *wdt);
uint16_t wdt0_read_counter(const volatile wdt0_registers_t *wdt);
void wdt0_write_control(volatile wdt0_registers_t *wdt, uint32_t value);
void wdt0_write_timeout(volatile wdt0_registers_t *wdt, uint16_t value);
void wdt0_write_window_open(
  volatile wdt0_registers_t *wdt,
  uint16_t value
);
void wdt0_write_feed(volatile wdt0_registers_t *wdt, uint32_t value);
void wdt0_write_status_clear(
  volatile wdt0_registers_t *wdt,
  uint32_t value
);
uint32_t wdt0_irq_save_disable(void);
void wdt0_irq_restore(uint32_t state);

#endif

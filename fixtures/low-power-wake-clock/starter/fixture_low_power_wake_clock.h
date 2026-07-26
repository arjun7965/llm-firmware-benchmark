#ifndef FIXTURE_LOW_POWER_WAKE_CLOCK_H
#define FIXTURE_LOW_POWER_WAKE_CLOCK_H

#include <stdint.h>

#define PWRCLK0_WAKE_RTC UINT32_C(1)
#define PWRCLK0_WAKE_GPIO (UINT32_C(1) << 1)
#define PWRCLK0_WAKE_UART (UINT32_C(1) << 2)
#define PWRCLK0_WAKE_ALL \
  (PWRCLK0_WAKE_RTC | PWRCLK0_WAKE_GPIO | PWRCLK0_WAKE_UART)

typedef struct pwrclk0_registers pwrclk0_registers_t;

typedef enum {
  PWRCLK0_CLOCK_RUN = 0,
  PWRCLK0_CLOCK_SLEEP,
  PWRCLK0_CLOCK_COUNT,
} pwrclk0_clock_t;

typedef enum {
  PWRCLK0_SLEEP_NONE = 0,
  PWRCLK0_SLEEP_IDLE,
  PWRCLK0_SLEEP_DEEP,
  PWRCLK0_SLEEP_COUNT,
} pwrclk0_sleep_t;

void pwrclk0_write_clock(
  volatile pwrclk0_registers_t *registers,
  pwrclk0_clock_t value
);
void pwrclk0_write_wake_enable(
  volatile pwrclk0_registers_t *registers,
  uint32_t value
);
uint32_t pwrclk0_read_wake_status(
  const volatile pwrclk0_registers_t *registers
);
void pwrclk0_write_wake_clear(
  volatile pwrclk0_registers_t *registers,
  uint32_t value
);
void pwrclk0_write_sleep_mode(
  volatile pwrclk0_registers_t *registers,
  pwrclk0_sleep_t value
);
uint32_t pwrclk0_irq_save_disable(void);
void pwrclk0_irq_restore(uint32_t state);

#endif

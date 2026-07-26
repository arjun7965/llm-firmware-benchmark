#ifndef LOW_POWER_WAKE_CLOCK_H
#define LOW_POWER_WAKE_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

#include "fixture_low_power_wake_clock.h"

#define POWER_RUN_CLOCK_HZ UINT32_C(48000000)
#define POWER_SLEEP_CLOCK_HZ UINT32_C(4000000)

typedef enum {
  POWER_SLEEP_MODE_IDLE = 0,
  POWER_SLEEP_MODE_DEEP,
  POWER_SLEEP_MODE_COUNT,
} power_sleep_mode_t;

typedef enum {
  POWER_WAKE_NONE = 0,
  POWER_WAKE_GPIO,
  POWER_WAKE_RTC,
  POWER_WAKE_UART,
} power_wake_reason_t;

typedef struct {
  volatile pwrclk0_registers_t *registers;
  uint32_t wake_mask;
  power_sleep_mode_t sleep_mode;
  bool sleeping;
  bool initialized;
} power_manager_t;

bool power_manager_init(
  power_manager_t *manager,
  volatile pwrclk0_registers_t *registers
);
bool power_manager_prepare_sleep(
  power_manager_t *manager,
  power_sleep_mode_t mode,
  uint32_t wake_mask
);
power_wake_reason_t power_manager_resume(power_manager_t *manager);
uint32_t power_manager_clock_hz(const power_manager_t *manager);

#endif

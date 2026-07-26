#include <stddef.h>

#include "low_power_wake_clock.h"

static bool mode_is_valid(power_sleep_mode_t mode) {
  return mode < POWER_SLEEP_MODE_COUNT;
}

static bool wake_mask_is_valid(uint32_t wake_mask) {
  return wake_mask != 0u && (wake_mask & ~PWRCLK0_WAKE_ALL) == 0u;
}

static bool wake_mask_supported(
  power_sleep_mode_t mode,
  uint32_t wake_mask
) {
  return mode != POWER_SLEEP_MODE_DEEP ||
    (wake_mask & PWRCLK0_WAKE_UART) == 0u;
}

static pwrclk0_sleep_t hardware_sleep_mode(power_sleep_mode_t mode) {
  return mode == POWER_SLEEP_MODE_DEEP
    ? PWRCLK0_SLEEP_DEEP
    : PWRCLK0_SLEEP_IDLE;
}

static power_wake_reason_t wake_reason(uint32_t observed) {
  if ((observed & PWRCLK0_WAKE_GPIO) != 0u) return POWER_WAKE_GPIO;
  if ((observed & PWRCLK0_WAKE_RTC) != 0u) return POWER_WAKE_RTC;
  if ((observed & PWRCLK0_WAKE_UART) != 0u) return POWER_WAKE_UART;
  return POWER_WAKE_NONE;
}

bool power_manager_init(
  power_manager_t *manager,
  volatile pwrclk0_registers_t *registers
) {
  if (manager == NULL || registers == NULL) return false;

  pwrclk0_write_wake_enable(registers, 0u);
  pwrclk0_write_wake_clear(registers, PWRCLK0_WAKE_ALL);
  pwrclk0_write_clock(registers, PWRCLK0_CLOCK_RUN);
  pwrclk0_write_sleep_mode(registers, PWRCLK0_SLEEP_NONE);
  *manager = (power_manager_t) {
    .registers = registers,
    .wake_mask = 0u,
    .sleep_mode = POWER_SLEEP_MODE_IDLE,
    .sleeping = false,
    .initialized = true,
  };
  return true;
}

bool power_manager_prepare_sleep(
  power_manager_t *manager,
  power_sleep_mode_t mode,
  uint32_t wake_mask
) {
  uint32_t irq_state;
  pwrclk0_clock_t clock;

  if (manager == NULL || !manager->initialized || manager->sleeping ||
      !mode_is_valid(mode) || !wake_mask_is_valid(wake_mask) ||
      !wake_mask_supported(mode, wake_mask)) {
    return false;
  }

  irq_state = pwrclk0_irq_save_disable();
  clock = mode == POWER_SLEEP_MODE_DEEP
    ? PWRCLK0_CLOCK_SLEEP
    : PWRCLK0_CLOCK_RUN;
  pwrclk0_write_wake_enable(manager->registers, 0u);
  pwrclk0_write_wake_clear(manager->registers, PWRCLK0_WAKE_ALL);
  pwrclk0_write_clock(manager->registers, clock);
  pwrclk0_write_wake_enable(manager->registers, wake_mask);
  pwrclk0_write_sleep_mode(manager->registers, hardware_sleep_mode(mode));
  manager->wake_mask = wake_mask;
  manager->sleep_mode = mode;
  manager->sleeping = true;
  pwrclk0_irq_restore(irq_state);
  return true;
}

power_wake_reason_t power_manager_resume(power_manager_t *manager) {
  uint32_t irq_state;
  uint32_t observed;
  power_wake_reason_t reason;

  if (manager == NULL || !manager->initialized || !manager->sleeping) {
    return POWER_WAKE_NONE;
  }

  irq_state = pwrclk0_irq_save_disable();
  observed = pwrclk0_read_wake_status(manager->registers) & manager->wake_mask;
  reason = wake_reason(observed);
  if (reason == POWER_WAKE_NONE) {
    pwrclk0_irq_restore(irq_state);
    return POWER_WAKE_NONE;
  }

  pwrclk0_write_wake_enable(manager->registers, 0u);
  pwrclk0_write_clock(manager->registers, PWRCLK0_CLOCK_RUN);
  pwrclk0_write_wake_clear(manager->registers, observed);
  pwrclk0_write_sleep_mode(manager->registers, PWRCLK0_SLEEP_NONE);
  manager->wake_mask = 0u;
  manager->sleeping = false;
  pwrclk0_irq_restore(irq_state);
  return reason;
}

uint32_t power_manager_clock_hz(const power_manager_t *manager) {
  if (manager == NULL || !manager->initialized) return 0u;
  return manager->sleeping && manager->sleep_mode == POWER_SLEEP_MODE_DEEP
    ? POWER_SLEEP_CLOCK_HZ
    : POWER_RUN_CLOCK_HZ;
}

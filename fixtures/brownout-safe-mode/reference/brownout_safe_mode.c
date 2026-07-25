#include <limits.h>
#include <stddef.h>

#include "brownout_safe_mode.h"

static uint32_t persistent_checksum(const brownout_persistent_t *persistent) {
  return persistent->magic ^ (uint32_t)persistent->brownout_count ^
    ((uint32_t)persistent->safe_mode << 24) ^
    BROWNOUT_PERSISTENT_CHECKSUM_XOR;
}

static bool persistent_is_valid(const brownout_persistent_t *persistent) {
  return persistent->magic == BROWNOUT_PERSISTENT_MAGIC &&
    persistent->safe_mode <= UINT8_C(1) && persistent->reserved == 0u &&
    persistent->checksum == persistent_checksum(persistent);
}

static void write_persistent(
  brownout_persistent_t *persistent,
  uint16_t brownout_count,
  bool safe_mode
) {
  *persistent = (brownout_persistent_t) {
    .magic = BROWNOUT_PERSISTENT_MAGIC,
    .brownout_count = brownout_count,
    .safe_mode = safe_mode ? UINT8_C(1) : UINT8_C(0),
    .reserved = 0u,
    .checksum = 0u,
  };
  persistent->checksum = persistent_checksum(persistent);
}

static bool manager_is_ready(const brownout_manager_t *manager) {
  return manager != NULL && manager->initialized && manager->pwr != NULL &&
    manager->persistent != NULL;
}

static bool configuration_is_valid(uint16_t low_mv, uint16_t recovery_mv) {
  return low_mv >= BROWNOUT_MINIMUM_MV && low_mv < BROWNOUT_MAXIMUM_MV &&
    recovery_mv > low_mv && recovery_mv <= BROWNOUT_MAXIMUM_MV;
}

static bool brownout_is_active(
  uint32_t status,
  uint16_t supply_mv,
  uint16_t low_mv
) {
  return (status & PWR0_STATUS_BROWNOUT) != 0u || supply_mv <= low_mv;
}

static uint16_t increment_count(uint16_t count) {
  return count < UINT16_MAX ? (uint16_t)(count + UINT16_C(1)) : count;
}

bool brownout_manager_init(
  brownout_manager_t *manager,
  volatile pwr0_registers_t *pwr,
  brownout_persistent_t *persistent,
  uint16_t low_mv,
  uint16_t recovery_mv
) {
  bool brownout_active;
  uint32_t status;
  uint16_t supply_mv;
  brownout_event_t event = BROWNOUT_EVENT_NONE;

  if (
    manager == NULL || pwr == NULL || persistent == NULL ||
    !configuration_is_valid(low_mv, recovery_mv)
  ) {
    return false;
  }

  if (!persistent_is_valid(persistent)) {
    write_persistent(persistent, 0u, false);
  }
  status = pwr0_read_status(pwr);
  supply_mv = pwr0_read_supply_mv(pwr);
  brownout_active = brownout_is_active(status, supply_mv, low_mv);
  if (brownout_active) {
    pwr0_write_load_control(pwr, PWR0_LOAD_SAFE);
    if (persistent->safe_mode == 0u) {
      write_persistent(persistent, increment_count(persistent->brownout_count), true);
    }
    if ((status & PWR0_STATUS_BROWNOUT) != 0u) {
      pwr0_write_status_clear(pwr, PWR0_STATUS_BROWNOUT);
    }
    event = BROWNOUT_EVENT_ENTERED_SAFE_MODE;
  } else if (persistent->safe_mode != 0u) {
    pwr0_write_load_control(pwr, PWR0_LOAD_SAFE);
  } else {
    pwr0_write_load_control(pwr, PWR0_LOAD_ENABLED);
  }

  *manager = (brownout_manager_t) {
    .pwr = pwr,
    .persistent = persistent,
    .low_mv = low_mv,
    .recovery_mv = recovery_mv,
    .event = event,
    .initialized = true,
  };
  return true;
}

brownout_event_t brownout_manager_poll(brownout_manager_t *manager) {
  bool brownout_active;
  bool entered_safe_mode = false;
  uint32_t irq_state;
  uint32_t status;
  uint16_t supply_mv;

  if (!manager_is_ready(manager)) return BROWNOUT_EVENT_NONE;

  irq_state = pwr0_irq_save_disable();
  status = pwr0_read_status(manager->pwr);
  supply_mv = pwr0_read_supply_mv(manager->pwr);
  brownout_active = brownout_is_active(status, supply_mv, manager->low_mv);
  if (brownout_active) {
    pwr0_write_load_control(manager->pwr, PWR0_LOAD_SAFE);
    if (manager->persistent->safe_mode == 0u) {
      write_persistent(
        manager->persistent,
        increment_count(manager->persistent->brownout_count),
        true
      );
      manager->event = BROWNOUT_EVENT_ENTERED_SAFE_MODE;
      entered_safe_mode = true;
    }
    if ((status & PWR0_STATUS_BROWNOUT) != 0u) {
      pwr0_write_status_clear(manager->pwr, PWR0_STATUS_BROWNOUT);
    }
  }
  pwr0_irq_restore(irq_state);
  return entered_safe_mode ? BROWNOUT_EVENT_ENTERED_SAFE_MODE
    : BROWNOUT_EVENT_NONE;
}

bool brownout_manager_resume(brownout_manager_t *manager) {
  uint32_t irq_state;
  uint32_t status;
  uint16_t supply_mv;

  if (!manager_is_ready(manager)) return false;

  irq_state = pwr0_irq_save_disable();
  if (
    manager->persistent->safe_mode == 0u ||
    manager->event != BROWNOUT_EVENT_NONE
  ) {
    pwr0_irq_restore(irq_state);
    return false;
  }
  status = pwr0_read_status(manager->pwr);
  supply_mv = pwr0_read_supply_mv(manager->pwr);
  if (
    (status & PWR0_STATUS_BROWNOUT) != 0u ||
    supply_mv < manager->recovery_mv
  ) {
    pwr0_write_load_control(manager->pwr, PWR0_LOAD_SAFE);
    pwr0_irq_restore(irq_state);
    return false;
  }

  pwr0_write_load_control(manager->pwr, PWR0_LOAD_ENABLED);
  write_persistent(
    manager->persistent,
    manager->persistent->brownout_count,
    false
  );
  manager->event = BROWNOUT_EVENT_RESUMED;
  pwr0_irq_restore(irq_state);
  return true;
}

brownout_event_t brownout_manager_take_event(brownout_manager_t *manager) {
  brownout_event_t event;
  uint32_t irq_state;

  if (!manager_is_ready(manager)) return BROWNOUT_EVENT_NONE;

  irq_state = pwr0_irq_save_disable();
  event = manager->event;
  manager->event = BROWNOUT_EVENT_NONE;
  pwr0_irq_restore(irq_state);
  return event;
}

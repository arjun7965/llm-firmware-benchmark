#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mpu_fault_containment.h"

static mpu0_region_config_t region_config(uint32_t index) {
  switch (index) {
    case MPU0_REGION_FLASH:
      return (mpu0_region_config_t) {
        .base = MPU0_FLASH_BASE,
        .size = MPU0_FLASH_SIZE,
        .priority = 0u,
        .permissions = MPU0_PERM_FLASH,
        .execute_never = MPU0_XN_FLASH,
      };
    case MPU0_REGION_SRAM:
      return (mpu0_region_config_t) {
        .base = MPU0_SRAM_BASE,
        .size = MPU0_SRAM_SIZE,
        .priority = 1u,
        .permissions = MPU0_PERM_SRAM,
        .execute_never = MPU0_XN_SRAM,
      };
    case MPU0_REGION_KEY_VAULT:
      return (mpu0_region_config_t) {
        .base = MPU0_KEY_VAULT_BASE,
        .size = MPU0_KEY_VAULT_SIZE,
        .priority = 2u,
        .permissions = MPU0_PERM_KEY_VAULT,
        .execute_never = MPU0_XN_KEY_VAULT,
      };
    default:
      return (mpu0_region_config_t) {
        .base = MPU0_STACK_GUARD_BASE,
        .size = MPU0_STACK_GUARD_SIZE,
        .priority = 3u,
        .permissions = MPU0_PERM_STACK_GUARD,
        .execute_never = MPU0_XN_STACK_GUARD,
      };
  }
}

static bool same_config(
  const mpu0_region_config_t *left,
  const mpu0_region_config_t *right
) {
  return left->base == right->base && left->size == right->size &&
    left->priority == right->priority &&
    left->permissions == right->permissions &&
    left->execute_never == right->execute_never;
}

static bool usable(const mpu_fault_containment_t *containment) {
  return containment != NULL && containment->initialized &&
    containment->mpu != NULL && containment->security != NULL;
}

static bool fail_closed(
  mpu_fault_containment_t *containment,
  uint32_t fault_bits
) {
  if (fault_bits != 0u) {
    secctl_contain_fault(containment->security, fault_bits);
  } else {
    secctl_contain_configuration(containment->security);
  }
  containment->ready = false;
  containment->contained = true;
  return false;
}

bool mpu_fault_containment_init(
  mpu_fault_containment_t *containment,
  volatile mpu0_registers_t *mpu,
  volatile security_controller_t *security
) {
  mpu0_region_config_t actual;
  uint32_t faults;
  bool read_ok;

  if (containment == NULL || mpu == NULL || security == NULL) return false;
  *containment = (mpu_fault_containment_t) {
    .mpu = mpu,
    .security = security,
    .initialized = true,
  };
  secctl_lock_debug(security);
  secctl_lock_update(security);
  secctl_seal_secrets(security);
  faults = mpu0_read_fault_status(mpu);
  if (faults != 0u) return fail_closed(containment, faults);

  mpu0_disable(mpu);
  for (uint32_t index = 0u; index < MPU0_REGION_COUNT; index++) {
    const mpu0_region_config_t expected = region_config(index);
    mpu0_program_region(mpu, index, &expected);
    faults = mpu0_read_fault_status(mpu);
    if (faults != 0u) return fail_closed(containment, faults);
  }
  mpu0_data_barrier(mpu);
  mpu0_instruction_barrier(mpu);
  mpu0_enable_default_deny(mpu);
  mpu0_data_barrier(mpu);
  mpu0_instruction_barrier(mpu);
  for (uint32_t index = 0u; index < MPU0_REGION_COUNT; index++) {
    const mpu0_region_config_t expected = region_config(index);
    read_ok = mpu0_read_region(mpu, index, &actual);
    faults = mpu0_read_fault_status(mpu);
    if (faults != 0u) return fail_closed(containment, faults);
    if (!read_ok || !same_config(&actual, &expected)) {
      return fail_closed(containment, 0u);
    }
  }
  read_ok = mpu0_read_enabled(mpu);
  faults = mpu0_read_fault_status(mpu);
  if (faults != 0u) return fail_closed(containment, faults);
  if (!read_ok) return fail_closed(containment, 0u);
  containment->ready = true;
  containment->contained = false;
  return true;
}

bool mpu_fault_containment_irq(mpu_fault_containment_t *containment) {
  uint32_t observed;

  if (!usable(containment) || !containment->ready || containment->contained) {
    return false;
  }
  observed = mpu0_read_fault_status(containment->mpu);
  if (observed == 0u) return false;
  secctl_contain_fault(containment->security, observed);
  mpu0_clear_faults(containment->mpu, observed);
  containment->event.fault_bits = observed;
  containment->event_pending = true;
  containment->ready = false;
  containment->contained = true;
  return true;
}

bool mpu_fault_containment_take_event(
  mpu_fault_containment_t *containment,
  mpu_fault_event_t *event
) {
  uint32_t previous;

  if (!usable(containment) || event == NULL || !containment->event_pending) {
    return false;
  }
  previous = secctl_save_interrupts(containment->security);
  *event = containment->event;
  containment->event_pending = false;
  secctl_restore_interrupts(containment->security, previous);
  return true;
}

bool mpu_fault_containment_ready(
  const mpu_fault_containment_t *containment
) {
  return containment != NULL && containment->ready;
}

bool mpu_fault_containment_contained(
  const mpu_fault_containment_t *containment
) {
  return containment != NULL && containment->contained;
}

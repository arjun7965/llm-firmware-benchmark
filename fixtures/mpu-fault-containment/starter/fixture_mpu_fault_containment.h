#ifndef FIXTURE_MPU_FAULT_CONTAINMENT_H
#define FIXTURE_MPU_FAULT_CONTAINMENT_H

#include <stdbool.h>
#include <stdint.h>

#define MPU0_REGION_COUNT 4u
#define MPU0_REGION_FLASH 0u
#define MPU0_REGION_SRAM 1u
#define MPU0_REGION_KEY_VAULT 2u
#define MPU0_REGION_STACK_GUARD 3u

#define MPU0_PERM_FLASH UINT32_C(0x11)
#define MPU0_PERM_SRAM UINT32_C(0x06)
#define MPU0_PERM_KEY_VAULT UINT32_C(0x02)
#define MPU0_PERM_STACK_GUARD UINT32_C(0)
#define MPU0_XN_FLASH false
#define MPU0_XN_SRAM true
#define MPU0_XN_KEY_VAULT true
#define MPU0_XN_STACK_GUARD true

#define MPU0_FLASH_BASE UINT32_C(0x08000000)
#define MPU0_FLASH_SIZE UINT32_C(0x00040000)
#define MPU0_SRAM_BASE UINT32_C(0x20000000)
#define MPU0_SRAM_SIZE UINT32_C(0x00010000)
#define MPU0_KEY_VAULT_BASE UINT32_C(0x2000f000)
#define MPU0_KEY_VAULT_SIZE UINT32_C(0x00001000)
#define MPU0_STACK_GUARD_BASE UINT32_C(0x2000e000)
#define MPU0_STACK_GUARD_SIZE UINT32_C(0x00001000)

#define MPU0_FAULT_CLOCK UINT32_C(1)
#define MPU0_FAULT_VOLTAGE UINT32_C(2)
#define MPU0_FAULT_GLITCH UINT32_C(4)
#define MPU0_FAULT_MPU UINT32_C(8)
#define MPU0_FAULT_ALL UINT32_C(15)

typedef struct mpu0_registers mpu0_registers_t;
typedef struct security_controller security_controller_t;

typedef struct {
  uint32_t base;
  uint32_t size;
  uint32_t priority;
  uint32_t permissions;
  bool execute_never;
} mpu0_region_config_t;

void secctl_lock_debug(volatile security_controller_t *security);
void secctl_lock_update(volatile security_controller_t *security);
void secctl_seal_secrets(volatile security_controller_t *security);
void secctl_contain_fault(
  volatile security_controller_t *security,
  uint32_t fault_bits
);
void secctl_contain_configuration(
  volatile security_controller_t *security
);
uint32_t secctl_save_interrupts(volatile security_controller_t *security);
void secctl_restore_interrupts(
  volatile security_controller_t *security,
  uint32_t previous
);

uint32_t mpu0_read_fault_status(const volatile mpu0_registers_t *mpu);
void mpu0_disable(volatile mpu0_registers_t *mpu);
void mpu0_program_region(
  volatile mpu0_registers_t *mpu,
  uint32_t index,
  const mpu0_region_config_t *config
);
void mpu0_data_barrier(volatile mpu0_registers_t *mpu);
void mpu0_instruction_barrier(volatile mpu0_registers_t *mpu);
void mpu0_enable_default_deny(volatile mpu0_registers_t *mpu);
bool mpu0_read_region(
  const volatile mpu0_registers_t *mpu,
  uint32_t index,
  mpu0_region_config_t *config
);
bool mpu0_read_enabled(const volatile mpu0_registers_t *mpu);
void mpu0_clear_faults(
  volatile mpu0_registers_t *mpu,
  uint32_t fault_bits
);

#endif

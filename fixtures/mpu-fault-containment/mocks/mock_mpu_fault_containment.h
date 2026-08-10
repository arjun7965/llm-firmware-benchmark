#ifndef MOCK_MPU_FAULT_CONTAINMENT_H
#define MOCK_MPU_FAULT_CONTAINMENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_mpu_fault_containment.h"

typedef enum {
  MOCK_MPU_EVENT_LOCK_DEBUG = 0,
  MOCK_MPU_EVENT_LOCK_UPDATE,
  MOCK_MPU_EVENT_SEAL_SECRETS,
  MOCK_MPU_EVENT_READ_FAULT,
  MOCK_MPU_EVENT_DISABLE,
  MOCK_MPU_EVENT_PROGRAM,
  MOCK_MPU_EVENT_DSB,
  MOCK_MPU_EVENT_ISB,
  MOCK_MPU_EVENT_ENABLE,
  MOCK_MPU_EVENT_READ_REGION,
  MOCK_MPU_EVENT_READ_ENABLE,
  MOCK_MPU_EVENT_CONTAIN,
  MOCK_MPU_EVENT_CONTAIN_CONFIGURATION,
  MOCK_MPU_EVENT_CLEAR,
  MOCK_MPU_EVENT_SAVE_IRQ,
  MOCK_MPU_EVENT_RESTORE_IRQ,
} mock_mpu_event_t;

typedef bool (*mock_mpu_state_validator_t)(const void *context);

#define MOCK_MPU_CORRUPT_BASE UINT32_C(1)
#define MOCK_MPU_CORRUPT_SIZE UINT32_C(2)
#define MOCK_MPU_CORRUPT_PRIORITY UINT32_C(4)
#define MOCK_MPU_CORRUPT_PERMISSIONS UINT32_C(8)
#define MOCK_MPU_CORRUPT_EXECUTE_NEVER UINT32_C(16)

volatile mpu0_registers_t *mock_mpu(void);
volatile security_controller_t *mock_security_controller(void);
void mock_mpu_reset(void);
void mock_mpu_set_first_access_validator(
  mock_mpu_state_validator_t validator,
  const void *context
);
bool mock_mpu_first_access_validated(void);
void mock_mpu_set_fault_status(uint32_t status);
void mock_mpu_set_readback_corrupt(uint32_t index, bool corrupt);
void mock_mpu_set_readback_corruption(uint32_t index, uint32_t fields);
void mock_mpu_set_fault_on_enable(uint32_t status);
void mock_mpu_set_fault_on_program(uint32_t call_index, uint32_t status);
void mock_mpu_set_fault_on_read_region(uint32_t call_index, uint32_t status);
void mock_mpu_set_read_region_failure(uint32_t call_index, bool fail);
void mock_mpu_set_fault_on_read_enable(uint32_t status);
void mock_mpu_set_enable_readback(bool enabled);
void mock_mpu_set_interrupt_state(uint32_t state);
size_t mock_mpu_event_count(void);
mock_mpu_event_t mock_mpu_event_at(size_t index);
uint32_t mock_mpu_event_value(size_t index);
uint32_t mock_mpu_contained_bits(void);
uint32_t mock_mpu_cleared_bits(void);
uint32_t mock_mpu_interrupt_state(void);
bool mock_mpu_configuration_contained(void);
bool mock_mpu_region(uint32_t index, mpu0_region_config_t *config);
bool mock_mpu_invalid_access(void);

#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mock_mpu_fault_containment.h"

struct mpu0_registers { uint32_t marker; };
struct security_controller { uint32_t marker; };

typedef struct {
  mock_mpu_event_t type;
  uint32_t value;
} event_t;

static struct mpu0_registers mpu = { UINT32_C(0x4d505530) };
static struct security_controller security = { UINT32_C(0x53454354) };
static event_t events[128];
static size_t event_count;
static uint32_t fault_status;
static uint32_t corruption[MPU0_REGION_COUNT];
static bool enabled;
static bool enable_readback = true;
static uint32_t fault_on_enable;
static uint32_t fault_on_program_call;
static uint32_t fault_on_program_status;
static uint32_t fault_on_read_region_call;
static uint32_t fault_on_read_region_status;
static uint32_t fault_on_read_enable;
static uint32_t program_calls;
static uint32_t read_region_calls;
static uint32_t read_region_failure_call;
static uint32_t contained_bits;
static uint32_t cleared_bits;
static uint32_t interrupt_state = UINT32_C(0xa5a5a5a5);
static bool configuration_contained;
static mpu0_region_config_t regions[MPU0_REGION_COUNT];

static bool good_mpu(const volatile mpu0_registers_t *value) {
  return value == &mpu && mpu.marker == UINT32_C(0x4d505530);
}
static bool good_security(const volatile security_controller_t *value) {
  return value == &security && security.marker == UINT32_C(0x53454354);
}
static void record(mock_mpu_event_t type, uint32_t value) {
  if (event_count < sizeof(events) / sizeof(events[0])) {
    events[event_count++] = (event_t) { type, value };
  }
}

volatile mpu0_registers_t *mock_mpu(void) { return &mpu; }
volatile security_controller_t *mock_security_controller(void) { return &security; }
void mock_mpu_reset(void) {
  event_count = 0u;
  fault_status = 0u;
  contained_bits = 0u;
  cleared_bits = 0u;
  enabled = false;
  enable_readback = true;
  fault_on_enable = 0u;
  fault_on_program_call = 0u;
  fault_on_program_status = 0u;
  fault_on_read_region_call = 0u;
  fault_on_read_region_status = 0u;
  fault_on_read_enable = 0u;
  program_calls = 0u;
  read_region_calls = 0u;
  read_region_failure_call = 0u;
  interrupt_state = UINT32_C(0xa5a5a5a5);
  configuration_contained = false;
  for (size_t index = 0u; index < MPU0_REGION_COUNT; index++) {
    corruption[index] = 0u;
    regions[index] = (mpu0_region_config_t) { 0 };
  }
}
void mock_mpu_set_fault_status(uint32_t status) { fault_status = status; }
void mock_mpu_set_readback_corrupt(uint32_t index, bool value) {
  if (index < MPU0_REGION_COUNT) {
    corruption[index] = value ? MOCK_MPU_CORRUPT_PERMISSIONS : 0u;
  }
}
void mock_mpu_set_readback_corruption(uint32_t index, uint32_t fields) {
  if (index < MPU0_REGION_COUNT) corruption[index] = fields;
}
void mock_mpu_set_fault_on_enable(uint32_t status) { fault_on_enable = status; }
void mock_mpu_set_fault_on_program(uint32_t call_index, uint32_t status) {
  fault_on_program_call = call_index;
  fault_on_program_status = status;
}
void mock_mpu_set_fault_on_read_region(uint32_t call_index, uint32_t status) {
  fault_on_read_region_call = call_index;
  fault_on_read_region_status = status;
}
void mock_mpu_set_read_region_failure(uint32_t call_index, bool fail) {
  read_region_failure_call = fail ? call_index : 0u;
}
void mock_mpu_set_fault_on_read_enable(uint32_t status) {
  fault_on_read_enable = status;
}
void mock_mpu_set_enable_readback(bool value) { enable_readback = value; }
void mock_mpu_set_interrupt_state(uint32_t value) { interrupt_state = value; }
size_t mock_mpu_event_count(void) { return event_count; }
mock_mpu_event_t mock_mpu_event_at(size_t index) { return events[index].type; }
uint32_t mock_mpu_event_value(size_t index) { return events[index].value; }
uint32_t mock_mpu_contained_bits(void) { return contained_bits; }
uint32_t mock_mpu_cleared_bits(void) { return cleared_bits; }
uint32_t mock_mpu_interrupt_state(void) { return interrupt_state; }
bool mock_mpu_configuration_contained(void) { return configuration_contained; }
bool mock_mpu_region(uint32_t index, mpu0_region_config_t *config) {
  if (index >= MPU0_REGION_COUNT || config == NULL) return false;
  *config = regions[index];
  return true;
}
bool mock_mpu_invalid_access(void) {
  return !good_mpu(&mpu) || !good_security(&security);
}

void secctl_lock_debug(volatile security_controller_t *value) {
  if (!good_security(value)) return;
  record(MOCK_MPU_EVENT_LOCK_DEBUG, 0u);
}
void secctl_lock_update(volatile security_controller_t *value) {
  if (!good_security(value)) return;
  record(MOCK_MPU_EVENT_LOCK_UPDATE, 0u);
}
void secctl_seal_secrets(volatile security_controller_t *value) {
  if (!good_security(value)) return;
  record(MOCK_MPU_EVENT_SEAL_SECRETS, 0u);
}
void secctl_contain_fault(volatile security_controller_t *value, uint32_t bits) {
  if (!good_security(value)) return;
  contained_bits |= bits;
  record(MOCK_MPU_EVENT_CONTAIN, bits);
}
void secctl_contain_configuration(volatile security_controller_t *value) {
  if (!good_security(value)) return;
  configuration_contained = true;
  record(MOCK_MPU_EVENT_CONTAIN_CONFIGURATION, MPU0_FAULT_MPU);
}
uint32_t secctl_save_interrupts(volatile security_controller_t *value) {
  if (!good_security(value)) return 0u;
  record(MOCK_MPU_EVENT_SAVE_IRQ, interrupt_state);
  return interrupt_state;
}
void secctl_restore_interrupts(volatile security_controller_t *value, uint32_t previous) {
  if (!good_security(value)) return;
  record(MOCK_MPU_EVENT_RESTORE_IRQ, previous);
  interrupt_state = previous;
}

uint32_t mpu0_read_fault_status(const volatile mpu0_registers_t *value) {
  if (!good_mpu(value)) return MPU0_FAULT_ALL;
  record(MOCK_MPU_EVENT_READ_FAULT, fault_status);
  return fault_status;
}
void mpu0_disable(volatile mpu0_registers_t *value) {
  if (!good_mpu(value)) return;
  enabled = false;
  record(MOCK_MPU_EVENT_DISABLE, 0u);
}
void mpu0_program_region(volatile mpu0_registers_t *value, uint32_t index, const mpu0_region_config_t *config) {
  if (!good_mpu(value) || config == NULL || index >= MPU0_REGION_COUNT) return;
  regions[index] = *config;
  program_calls++;
  if (program_calls == fault_on_program_call) {
    fault_status |= fault_on_program_status;
  }
  record(MOCK_MPU_EVENT_PROGRAM, index);
}
void mpu0_data_barrier(volatile mpu0_registers_t *value) {
  if (good_mpu(value)) record(MOCK_MPU_EVENT_DSB, 0u);
}
void mpu0_instruction_barrier(volatile mpu0_registers_t *value) {
  if (good_mpu(value)) record(MOCK_MPU_EVENT_ISB, 0u);
}
void mpu0_enable_default_deny(volatile mpu0_registers_t *value) {
  if (!good_mpu(value)) return;
  enabled = true;
  fault_status |= fault_on_enable;
  record(MOCK_MPU_EVENT_ENABLE, 0u);
}
bool mpu0_read_region(const volatile mpu0_registers_t *value, uint32_t index, mpu0_region_config_t *config) {
  if (!good_mpu(value) || config == NULL || index >= MPU0_REGION_COUNT) return false;
  read_region_calls++;
  if (read_region_calls == fault_on_read_region_call) {
    fault_status |= fault_on_read_region_status;
  }
  if (read_region_calls == read_region_failure_call) {
    record(MOCK_MPU_EVENT_READ_REGION, index);
    return false;
  }
  *config = regions[index];
  if ((corruption[index] & MOCK_MPU_CORRUPT_BASE) != 0u) {
    config->base ^= UINT32_C(1);
  }
  if ((corruption[index] & MOCK_MPU_CORRUPT_SIZE) != 0u) {
    config->size ^= UINT32_C(1);
  }
  if ((corruption[index] & MOCK_MPU_CORRUPT_PRIORITY) != 0u) {
    config->priority ^= UINT32_C(1);
  }
  if ((corruption[index] & MOCK_MPU_CORRUPT_PERMISSIONS) != 0u) {
    config->permissions ^= UINT32_C(1);
  }
  if ((corruption[index] & MOCK_MPU_CORRUPT_EXECUTE_NEVER) != 0u) {
    config->execute_never = !config->execute_never;
  }
  record(MOCK_MPU_EVENT_READ_REGION, index);
  return true;
}
bool mpu0_read_enabled(const volatile mpu0_registers_t *value) {
  if (!good_mpu(value)) return false;
  fault_status |= fault_on_read_enable;
  record(MOCK_MPU_EVENT_READ_ENABLE, enabled ? 1u : 0u);
  return enable_readback && enabled;
}
void mpu0_clear_faults(volatile mpu0_registers_t *value, uint32_t bits) {
  if (!good_mpu(value)) return;
  cleared_bits |= bits;
  fault_status &= ~bits;
  record(MOCK_MPU_EVENT_CLEAR, bits);
}

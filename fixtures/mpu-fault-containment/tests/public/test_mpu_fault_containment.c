#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "mock_mpu_fault_containment.h"
#include "mpu_fault_containment.h"

#define CHECK(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
    return false; \
  } \
} while (false)

static bool initialize(mpu_fault_containment_t *containment) {
  mock_mpu_reset();
  return mpu_fault_containment_init(
    containment, mock_mpu(), mock_security_controller());
}

static bool state_equals(
  const mpu_fault_containment_t *left,
  const mpu_fault_containment_t *right
) {
  return left->mpu == right->mpu && left->security == right->security &&
    left->event.fault_bits == right->event.fault_bits &&
    left->initialized == right->initialized && left->ready == right->ready &&
    left->contained == right->contained &&
    left->event_pending == right->event_pending;
}

static bool same_config(
  const mpu0_region_config_t *config,
  uint32_t base,
  uint32_t size,
  uint32_t priority,
  uint32_t permissions,
  bool execute_never
) {
  return config->base == base && config->size == size &&
    config->priority == priority && config->permissions == permissions &&
    config->execute_never == execute_never;
}

static bool test_safe_first_exact_policy(void) {
  mpu_fault_containment_t containment = { 0 };
  const mock_mpu_event_t expected[] = {
    MOCK_MPU_EVENT_LOCK_DEBUG, MOCK_MPU_EVENT_LOCK_UPDATE,
    MOCK_MPU_EVENT_SEAL_SECRETS, MOCK_MPU_EVENT_READ_FAULT,
    MOCK_MPU_EVENT_DISABLE,
    MOCK_MPU_EVENT_PROGRAM, MOCK_MPU_EVENT_READ_FAULT,
    MOCK_MPU_EVENT_PROGRAM, MOCK_MPU_EVENT_READ_FAULT,
    MOCK_MPU_EVENT_PROGRAM, MOCK_MPU_EVENT_READ_FAULT,
    MOCK_MPU_EVENT_PROGRAM, MOCK_MPU_EVENT_READ_FAULT,
    MOCK_MPU_EVENT_DSB, MOCK_MPU_EVENT_ISB,
    MOCK_MPU_EVENT_ENABLE, MOCK_MPU_EVENT_DSB, MOCK_MPU_EVENT_ISB,
    MOCK_MPU_EVENT_READ_REGION, MOCK_MPU_EVENT_READ_FAULT,
    MOCK_MPU_EVENT_READ_REGION, MOCK_MPU_EVENT_READ_FAULT,
    MOCK_MPU_EVENT_READ_REGION, MOCK_MPU_EVENT_READ_FAULT,
    MOCK_MPU_EVENT_READ_REGION, MOCK_MPU_EVENT_READ_FAULT,
    MOCK_MPU_EVENT_READ_ENABLE, MOCK_MPU_EVENT_READ_FAULT,
  };
  mpu0_region_config_t config;

  CHECK(initialize(&containment));
  CHECK(mpu_fault_containment_ready(&containment));
  CHECK(!mpu_fault_containment_contained(&containment));
  CHECK(mock_mpu_event_count() == sizeof(expected) / sizeof(expected[0]));
  for (size_t index = 0u; index < sizeof(expected) / sizeof(expected[0]); index++) {
    CHECK(mock_mpu_event_at(index) == expected[index]);
  }
  CHECK(mock_mpu_region(MPU0_REGION_FLASH, &config));
  CHECK(same_config(&config, MPU0_FLASH_BASE, MPU0_FLASH_SIZE, 0u,
    MPU0_PERM_FLASH, MPU0_XN_FLASH));
  CHECK(mock_mpu_region(MPU0_REGION_SRAM, &config));
  CHECK(same_config(&config, MPU0_SRAM_BASE, MPU0_SRAM_SIZE, 1u,
    MPU0_PERM_SRAM, MPU0_XN_SRAM));
  CHECK(mock_mpu_region(MPU0_REGION_KEY_VAULT, &config));
  CHECK(same_config(&config, MPU0_KEY_VAULT_BASE, MPU0_KEY_VAULT_SIZE, 2u,
    MPU0_PERM_KEY_VAULT, MPU0_XN_KEY_VAULT));
  CHECK(mock_mpu_region(MPU0_REGION_STACK_GUARD, &config));
  CHECK(same_config(&config, MPU0_STACK_GUARD_BASE, MPU0_STACK_GUARD_SIZE, 3u,
    MPU0_PERM_STACK_GUARD, MPU0_XN_STACK_GUARD));
  CHECK(!mock_mpu_invalid_access());
  return true;
}

static bool test_invalid_and_fault_injection_fails_closed(void) {
  mpu_fault_containment_t containment = {
    .mpu = (volatile mpu0_registers_t *)(uintptr_t)1u,
    .security = (volatile security_controller_t *)(uintptr_t)2u,
    .initialized = true,
    .ready = true,
  };
  const mpu_fault_containment_t before = containment;

  mock_mpu_reset();
  CHECK(!mpu_fault_containment_init(NULL, mock_mpu(), mock_security_controller()));
  CHECK(!mpu_fault_containment_init(&containment, NULL, mock_security_controller()));
  CHECK(!mpu_fault_containment_init(&containment, mock_mpu(), NULL));
  CHECK(state_equals(&containment, &before));
  CHECK(mock_mpu_event_count() == 0u);

  mock_mpu_set_fault_status(MPU0_FAULT_CLOCK | MPU0_FAULT_GLITCH);
  CHECK(!mpu_fault_containment_init(&containment, mock_mpu(), mock_security_controller()));
  CHECK(!mpu_fault_containment_ready(&containment));
  CHECK(mpu_fault_containment_contained(&containment));
  CHECK(mock_mpu_contained_bits() == (MPU0_FAULT_CLOCK | MPU0_FAULT_GLITCH));
  CHECK(mock_mpu_cleared_bits() == 0u);

  mock_mpu_reset();
  mock_mpu_set_fault_on_program(2u, MPU0_FAULT_VOLTAGE);
  CHECK(!mpu_fault_containment_init(&containment, mock_mpu(), mock_security_controller()));
  CHECK(!mpu_fault_containment_ready(&containment));
  CHECK(mock_mpu_contained_bits() == MPU0_FAULT_VOLTAGE);

  mock_mpu_reset();
  mock_mpu_set_fault_on_read_region(3u, MPU0_FAULT_GLITCH);
  CHECK(!mpu_fault_containment_init(&containment, mock_mpu(), mock_security_controller()));
  CHECK(!mpu_fault_containment_ready(&containment));
  CHECK(mock_mpu_contained_bits() == MPU0_FAULT_GLITCH);

  mock_mpu_reset();
  mock_mpu_set_fault_on_enable(MPU0_FAULT_MPU);
  CHECK(!mpu_fault_containment_init(&containment, mock_mpu(), mock_security_controller()));
  CHECK(!mpu_fault_containment_ready(&containment));
  CHECK(mock_mpu_contained_bits() == MPU0_FAULT_MPU);
  return true;
}

static bool test_every_injection_boundary_fails_closed(void) {
  mpu_fault_containment_t containment = { 0 };

  for (uint32_t call = 1u; call <= MPU0_REGION_COUNT; call++) {
    const uint32_t fault = UINT32_C(1) << ((call - 1u) % 4u);
    mock_mpu_reset();
    mock_mpu_set_fault_on_program(call, fault);
    CHECK(!mpu_fault_containment_init(
      &containment, mock_mpu(), mock_security_controller()
    ));
    CHECK(!mpu_fault_containment_ready(&containment));
    CHECK(mpu_fault_containment_contained(&containment));
    CHECK(mock_mpu_contained_bits() == fault);
    CHECK(mock_mpu_cleared_bits() == 0u);
    CHECK(mock_mpu_event_at(mock_mpu_event_count() - 3u) ==
      MOCK_MPU_EVENT_PROGRAM);
    CHECK(mock_mpu_event_at(mock_mpu_event_count() - 2u) ==
      MOCK_MPU_EVENT_READ_FAULT);
    CHECK(mock_mpu_event_at(mock_mpu_event_count() - 1u) ==
      MOCK_MPU_EVENT_CONTAIN);
  }

  for (uint32_t call = 1u; call <= MPU0_REGION_COUNT; call++) {
    const uint32_t fault = UINT32_C(1) << (call % 4u);
    mock_mpu_reset();
    mock_mpu_set_fault_on_read_region(call, fault);
    CHECK(!mpu_fault_containment_init(
      &containment, mock_mpu(), mock_security_controller()
    ));
    CHECK(!mpu_fault_containment_ready(&containment));
    CHECK(mpu_fault_containment_contained(&containment));
    CHECK(mock_mpu_contained_bits() == fault);
    CHECK(mock_mpu_cleared_bits() == 0u);
    CHECK(mock_mpu_event_at(mock_mpu_event_count() - 3u) ==
      MOCK_MPU_EVENT_READ_REGION);
    CHECK(mock_mpu_event_at(mock_mpu_event_count() - 2u) ==
      MOCK_MPU_EVENT_READ_FAULT);
    CHECK(mock_mpu_event_at(mock_mpu_event_count() - 1u) ==
      MOCK_MPU_EVENT_CONTAIN);
  }

  mock_mpu_reset();
  mock_mpu_set_fault_on_enable(MPU0_FAULT_MPU);
  CHECK(!mpu_fault_containment_init(
    &containment, mock_mpu(), mock_security_controller()
  ));
  CHECK(mock_mpu_contained_bits() == MPU0_FAULT_MPU);
  CHECK(mock_mpu_cleared_bits() == 0u);

  mock_mpu_reset();
  mock_mpu_set_fault_on_read_enable(MPU0_FAULT_CLOCK | MPU0_FAULT_VOLTAGE);
  CHECK(!mpu_fault_containment_init(
    &containment, mock_mpu(), mock_security_controller()
  ));
  CHECK(mock_mpu_contained_bits() ==
    (MPU0_FAULT_CLOCK | MPU0_FAULT_VOLTAGE));
  CHECK(mock_mpu_cleared_bits() == 0u);
  return true;
}

static bool test_every_readback_field_fails_closed(void) {
  const uint32_t fields[] = {
    MOCK_MPU_CORRUPT_BASE,
    MOCK_MPU_CORRUPT_SIZE,
    MOCK_MPU_CORRUPT_PRIORITY,
    MOCK_MPU_CORRUPT_PERMISSIONS,
    MOCK_MPU_CORRUPT_EXECUTE_NEVER,
  };
  mpu_fault_containment_t containment = { 0 };

  for (size_t index = 0u; index < sizeof(fields) / sizeof(fields[0]); index++) {
    mock_mpu_reset();
    mock_mpu_set_readback_corruption(
      (uint32_t)(index % MPU0_REGION_COUNT), fields[index]
    );
    CHECK(!mpu_fault_containment_init(
      &containment, mock_mpu(), mock_security_controller()
    ));
    CHECK(!mpu_fault_containment_ready(&containment));
    CHECK(mpu_fault_containment_contained(&containment));
    CHECK(mock_mpu_configuration_contained());
    CHECK(mock_mpu_contained_bits() == 0u);
    CHECK(mock_mpu_cleared_bits() == 0u);
    CHECK(mock_mpu_event_at(mock_mpu_event_count() - 1u) ==
      MOCK_MPU_EVENT_CONTAIN_CONFIGURATION);
  }
  return true;
}

static bool test_readback_and_enable_fail_closed(void) {
  mpu_fault_containment_t containment = { 0 };

  mock_mpu_reset();
  mock_mpu_set_readback_corrupt(MPU0_REGION_KEY_VAULT, true);
  CHECK(!mpu_fault_containment_init(&containment, mock_mpu(), mock_security_controller()));
  CHECK(mpu_fault_containment_contained(&containment));
  CHECK(mock_mpu_configuration_contained());
  CHECK(mock_mpu_cleared_bits() == 0u);

  mock_mpu_reset();
  mock_mpu_set_read_region_failure(2u, true);
  CHECK(!mpu_fault_containment_init(&containment, mock_mpu(), mock_security_controller()));
  CHECK(mock_mpu_configuration_contained());

  mock_mpu_reset();
  mock_mpu_set_read_region_failure(2u, true);
  mock_mpu_set_fault_on_read_region(2u, MPU0_FAULT_GLITCH);
  CHECK(!mpu_fault_containment_init(&containment, mock_mpu(), mock_security_controller()));
  CHECK(!mock_mpu_configuration_contained());
  CHECK(mock_mpu_contained_bits() == MPU0_FAULT_GLITCH);
  CHECK(mock_mpu_cleared_bits() == 0u);
  CHECK(mock_mpu_event_at(mock_mpu_event_count() - 3u) ==
    MOCK_MPU_EVENT_READ_REGION);
  CHECK(mock_mpu_event_at(mock_mpu_event_count() - 2u) ==
    MOCK_MPU_EVENT_READ_FAULT);
  CHECK(mock_mpu_event_at(mock_mpu_event_count() - 1u) ==
    MOCK_MPU_EVENT_CONTAIN);
  CHECK(mock_mpu_event_value(mock_mpu_event_count() - 1u) ==
    MPU0_FAULT_GLITCH);

  mock_mpu_reset();
  mock_mpu_set_enable_readback(false);
  CHECK(!mpu_fault_containment_init(&containment, mock_mpu(), mock_security_controller()));
  CHECK(mock_mpu_configuration_contained());

  mock_mpu_reset();
  mock_mpu_set_fault_on_read_enable(MPU0_FAULT_CLOCK);
  CHECK(!mpu_fault_containment_init(&containment, mock_mpu(), mock_security_controller()));
  CHECK(mock_mpu_contained_bits() == MPU0_FAULT_CLOCK);

  mock_mpu_reset();
  mock_mpu_set_enable_readback(false);
  mock_mpu_set_fault_on_read_enable(MPU0_FAULT_VOLTAGE);
  CHECK(!mpu_fault_containment_init(&containment, mock_mpu(), mock_security_controller()));
  CHECK(!mock_mpu_configuration_contained());
  CHECK(mock_mpu_contained_bits() == MPU0_FAULT_VOLTAGE);
  CHECK(mock_mpu_cleared_bits() == 0u);
  CHECK(mock_mpu_event_at(mock_mpu_event_count() - 3u) ==
    MOCK_MPU_EVENT_READ_ENABLE);
  CHECK(mock_mpu_event_at(mock_mpu_event_count() - 2u) ==
    MOCK_MPU_EVENT_READ_FAULT);
  CHECK(mock_mpu_event_at(mock_mpu_event_count() - 1u) ==
    MOCK_MPU_EVENT_CONTAIN);
  CHECK(mock_mpu_event_value(mock_mpu_event_count() - 1u) ==
    MPU0_FAULT_VOLTAGE);
  return true;
}

static bool test_irq_contains_before_clear_and_restores_exact_irq_state(void) {
  mpu_fault_containment_t containment = { 0 };
  mpu_fault_event_t event = { 0 };
  size_t before;

  CHECK(initialize(&containment));
  mock_mpu_set_fault_status(MPU0_FAULT_ALL);
  before = mock_mpu_event_count();
  CHECK(mpu_fault_containment_irq(&containment));
  CHECK(mock_mpu_event_at(before) == MOCK_MPU_EVENT_READ_FAULT);
  CHECK(mock_mpu_event_at(before + 1u) == MOCK_MPU_EVENT_CONTAIN);
  CHECK(mock_mpu_event_at(before + 2u) == MOCK_MPU_EVENT_CLEAR);
  CHECK(mock_mpu_event_value(before + 1u) == MPU0_FAULT_ALL);
  CHECK(mock_mpu_event_value(before + 2u) == MPU0_FAULT_ALL);
  CHECK(mock_mpu_cleared_bits() == MPU0_FAULT_ALL);
  CHECK(!mpu_fault_containment_ready(&containment));
  CHECK(mpu_fault_containment_contained(&containment));

  mock_mpu_set_interrupt_state(UINT32_C(0x13579bdf));
  CHECK(mpu_fault_containment_take_event(&containment, &event));
  CHECK(event.fault_bits == MPU0_FAULT_ALL);
  CHECK(mock_mpu_event_at(mock_mpu_event_count() - 2u) == MOCK_MPU_EVENT_SAVE_IRQ);
  CHECK(mock_mpu_event_at(mock_mpu_event_count() - 1u) == MOCK_MPU_EVENT_RESTORE_IRQ);
  CHECK(mock_mpu_event_value(mock_mpu_event_count() - 2u) == UINT32_C(0x13579bdf));
  CHECK(mock_mpu_event_value(mock_mpu_event_count() - 1u) == UINT32_C(0x13579bdf));
  CHECK(mock_mpu_interrupt_state() == UINT32_C(0x13579bdf));
  CHECK(!mpu_fault_containment_take_event(&containment, &event));
  CHECK(!mpu_fault_containment_irq(&containment));
  return true;
}

static bool test_empty_irq_and_invalid_take_have_no_effect(void) {
  mpu_fault_containment_t containment = { 0 };
  mpu_fault_event_t event = { 0 };
  size_t before;

  CHECK(initialize(&containment));
  before = mock_mpu_event_count();
  CHECK(!mpu_fault_containment_irq(&containment));
  CHECK(mock_mpu_event_count() == before + 1u);
  CHECK(!mpu_fault_containment_take_event(&containment, NULL));
  CHECK(!mpu_fault_containment_take_event(NULL, &event));
  CHECK(mock_mpu_event_count() == before + 1u);
  return true;
}

int main(void) {
  const struct { const char *name; bool (*run)(void); } tests[] = {
    { "safe first exact policy", test_safe_first_exact_policy },
    { "invalid and injected faults", test_invalid_and_fault_injection_fails_closed },
    { "every injection boundary", test_every_injection_boundary_fails_closed },
    { "every readback field", test_every_readback_field_fails_closed },
    { "readback and enable failures", test_readback_and_enable_fail_closed },
    { "runtime containment and IRQ state", test_irq_contains_before_clear_and_restores_exact_irq_state },
    { "empty and invalid calls", test_empty_irq_and_invalid_take_have_no_effect },
  };
  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) {
      fprintf(stderr, "failed: %s\n", tests[index].name);
      return 1;
    }
  }
  printf("MPU fault-containment public tests passed (%zu tests).\n",
    sizeof(tests) / sizeof(tests[0]));
  return 0;
}

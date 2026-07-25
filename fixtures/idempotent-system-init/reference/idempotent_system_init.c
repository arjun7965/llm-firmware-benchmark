#include <limits.h>
#include <stddef.h>

#include "idempotent_system_init.h"

static uint32_t config_signature(const system_init_config_t *config) {
  return config->clock_hz ^ (config->peripheral_mask << 16) ^
    SYSTEM_INIT_SIGNATURE_XOR;
}

static uint32_t persistent_checksum(const system_init_persistent_t *persistent) {
  return persistent->magic ^ persistent->config_signature ^
    (uint32_t)persistent->successful_boots ^
    ((uint32_t)persistent->safe_mode << 24) ^
    SYSTEM_INIT_PERSISTENT_CHECKSUM_XOR;
}

static bool persistent_is_valid(const system_init_persistent_t *persistent) {
  return persistent->magic == SYSTEM_INIT_PERSISTENT_MAGIC &&
    persistent->safe_mode <= UINT8_C(1) && persistent->reserved == 0u &&
    persistent->checksum == persistent_checksum(persistent);
}

static void write_persistent(
  system_init_persistent_t *persistent,
  uint32_t signature,
  uint16_t successful_boots,
  bool safe_mode
) {
  *persistent = (system_init_persistent_t) {
    .magic = SYSTEM_INIT_PERSISTENT_MAGIC,
    .config_signature = signature,
    .successful_boots = successful_boots,
    .safe_mode = safe_mode ? UINT8_C(1) : UINT8_C(0),
    .reserved = 0u,
    .checksum = 0u,
  };
  persistent->checksum = persistent_checksum(persistent);
}

static bool config_is_valid(const system_init_config_t *config) {
  return config != NULL && config->clock_hz >= SYSTEM0_MIN_CLOCK_HZ &&
    config->clock_hz <= SYSTEM0_MAX_CLOCK_HZ &&
    config->clock_hz % SYSTEM0_CLOCK_STEP_HZ == 0u &&
    config->peripheral_mask != 0u &&
    (config->peripheral_mask & ~SYSTEM0_PERIPHERAL_MASK_ALL) == 0u;
}

static bool configs_equal(
  const system_init_config_t *left,
  const system_init_config_t *right
) {
  return left->clock_hz == right->clock_hz &&
    left->peripheral_mask == right->peripheral_mask;
}

static bool state_is_ready(const system_init_t *state) {
  return state != NULL && state->initialized && state->system != NULL &&
    state->persistent != NULL;
}

static uint16_t increment_boots(uint16_t boots) {
  return boots < UINT16_MAX ? (uint16_t)(boots + UINT16_C(1)) : boots;
}

static void program_ready(
  volatile system0_registers_t *system,
  const system_init_config_t *config
) {
  system0_write_control(system, SYSTEM0_CONTROL_SAFE);
  system0_write_clock_hz(system, config->clock_hz);
  system0_write_peripheral_mask(system, config->peripheral_mask);
  system0_write_control(system, SYSTEM0_CONTROL_READY);
}

system_init_result_t system_init_initialize(
  system_init_t *state,
  volatile system0_registers_t *system,
  system_init_persistent_t *persistent,
  const system_init_config_t *config
) {
  uint32_t signature;

  if (state == NULL || system == NULL || persistent == NULL ||
    !config_is_valid(config)) {
    return SYSTEM_INIT_RESULT_INVALID;
  }
  if (state->initialized) {
    if (
      state->system != system || state->persistent != persistent ||
      !configs_equal(&state->config, config)
    ) {
      return SYSTEM_INIT_RESULT_CONFLICT;
    }
    return state->safe_mode
      ? SYSTEM_INIT_RESULT_SAFE_MODE_LATCHED
      : SYSTEM_INIT_RESULT_ALREADY_READY;
  }

  signature = config_signature(config);
  if (!persistent_is_valid(persistent)) {
    write_persistent(persistent, signature, 0u, false);
  }
  if (persistent->safe_mode != 0u) {
    system0_write_control(system, SYSTEM0_CONTROL_SAFE);
    *state = (system_init_t) {
      .system = system,
      .persistent = persistent,
      .config = *config,
      .event = SYSTEM_INIT_EVENT_ENTERED_SAFE_MODE,
      .safe_mode = true,
      .initialized = true,
    };
    return SYSTEM_INIT_RESULT_SAFE_MODE_LATCHED;
  }

  program_ready(system, config);
  write_persistent(
    persistent,
    signature,
    increment_boots(persistent->successful_boots),
    false
  );
  *state = (system_init_t) {
    .system = system,
    .persistent = persistent,
    .config = *config,
    .event = SYSTEM_INIT_EVENT_INITIALIZED,
    .safe_mode = false,
    .initialized = true,
  };
  return SYSTEM_INIT_RESULT_CONFIGURED;
}

bool system_init_enter_safe_mode(system_init_t *state) {
  uint32_t irq_state;

  if (!state_is_ready(state)) return false;

  irq_state = system0_irq_save_disable();
  if (state->safe_mode || state->event != SYSTEM_INIT_EVENT_NONE) {
    system0_irq_restore(irq_state);
    return false;
  }
  system0_write_control(state->system, SYSTEM0_CONTROL_SAFE);
  write_persistent(
    state->persistent,
    config_signature(&state->config),
    state->persistent->successful_boots,
    true
  );
  state->safe_mode = true;
  state->event = SYSTEM_INIT_EVENT_ENTERED_SAFE_MODE;
  system0_irq_restore(irq_state);
  return true;
}

bool system_init_resume(system_init_t *state) {
  uint32_t irq_state;

  if (!state_is_ready(state)) return false;

  irq_state = system0_irq_save_disable();
  if (!state->safe_mode || state->event != SYSTEM_INIT_EVENT_NONE) {
    system0_irq_restore(irq_state);
    return false;
  }
  if (!persistent_is_valid(state->persistent)) {
    system0_write_control(state->system, SYSTEM0_CONTROL_SAFE);
    system0_irq_restore(irq_state);
    return false;
  }
  program_ready(state->system, &state->config);
  write_persistent(
    state->persistent,
    config_signature(&state->config),
    state->persistent->successful_boots,
    false
  );
  state->safe_mode = false;
  state->event = SYSTEM_INIT_EVENT_RESUMED;
  system0_irq_restore(irq_state);
  return true;
}

system_init_event_t system_init_take_event(system_init_t *state) {
  system_init_event_t event;
  uint32_t irq_state;

  if (!state_is_ready(state)) return SYSTEM_INIT_EVENT_NONE;

  irq_state = system0_irq_save_disable();
  event = state->event;
  state->event = SYSTEM_INIT_EVENT_NONE;
  system0_irq_restore(irq_state);
  return event;
}

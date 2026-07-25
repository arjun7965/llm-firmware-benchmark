#ifndef IDEMPOTENT_SYSTEM_INIT_H
#define IDEMPOTENT_SYSTEM_INIT_H

#include <stdbool.h>
#include <stdint.h>

#include "fixture_idempotent_system_init.h"

#define SYSTEM_INIT_PERSISTENT_MAGIC UINT32_C(0x53494E49)
#define SYSTEM_INIT_PERSISTENT_CHECKSUM_XOR UINT32_C(0xD7154A39)
#define SYSTEM_INIT_SIGNATURE_XOR UINT32_C(0x4B1D8267)

typedef enum {
  SYSTEM_INIT_RESULT_INVALID = 0,
  SYSTEM_INIT_RESULT_CONFIGURED,
  SYSTEM_INIT_RESULT_ALREADY_READY,
  SYSTEM_INIT_RESULT_SAFE_MODE_LATCHED,
  SYSTEM_INIT_RESULT_CONFLICT,
} system_init_result_t;

typedef enum {
  SYSTEM_INIT_EVENT_NONE = 0,
  SYSTEM_INIT_EVENT_INITIALIZED,
  SYSTEM_INIT_EVENT_ENTERED_SAFE_MODE,
  SYSTEM_INIT_EVENT_RESUMED,
} system_init_event_t;

typedef struct {
  uint32_t clock_hz;
  uint32_t peripheral_mask;
} system_init_config_t;

typedef struct {
  uint32_t magic;
  uint32_t config_signature;
  uint16_t successful_boots;
  uint8_t safe_mode;
  uint8_t reserved;
  uint32_t checksum;
} system_init_persistent_t;

typedef struct {
  volatile system0_registers_t *system;
  system_init_persistent_t *persistent;
  system_init_config_t config;
  system_init_event_t event;
  bool safe_mode;
  bool initialized;
} system_init_t;

system_init_result_t system_init_initialize(
  system_init_t *state,
  volatile system0_registers_t *system,
  system_init_persistent_t *persistent,
  const system_init_config_t *config
);
bool system_init_enter_safe_mode(system_init_t *state);
bool system_init_resume(system_init_t *state);
system_init_event_t system_init_take_event(system_init_t *state);

#endif

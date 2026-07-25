#ifndef BROWNOUT_SAFE_MODE_H
#define BROWNOUT_SAFE_MODE_H

#include <stdbool.h>
#include <stdint.h>

#include "fixture_brownout_safe_mode.h"

#define BROWNOUT_PERSISTENT_MAGIC UINT32_C(0x42524F57)
#define BROWNOUT_PERSISTENT_CHECKSUM_XOR UINT32_C(0xA53C91E7)

typedef enum {
  BROWNOUT_EVENT_NONE = 0,
  BROWNOUT_EVENT_ENTERED_SAFE_MODE,
  BROWNOUT_EVENT_RESUMED,
} brownout_event_t;

typedef struct {
  uint32_t magic;
  uint16_t brownout_count;
  uint8_t safe_mode;
  uint8_t reserved;
  uint32_t checksum;
} brownout_persistent_t;

typedef struct {
  volatile pwr0_registers_t *pwr;
  brownout_persistent_t *persistent;
  uint16_t low_mv;
  uint16_t recovery_mv;
  brownout_event_t event;
  bool initialized;
} brownout_manager_t;

bool brownout_manager_init(
  brownout_manager_t *manager,
  volatile pwr0_registers_t *pwr,
  brownout_persistent_t *persistent,
  uint16_t low_mv,
  uint16_t recovery_mv
);
brownout_event_t brownout_manager_poll(brownout_manager_t *manager);
bool brownout_manager_resume(brownout_manager_t *manager);
brownout_event_t brownout_manager_take_event(brownout_manager_t *manager);

#endif

#ifndef SECURE_MAINTENANCE_COMMAND_H
#define SECURE_MAINTENANCE_COMMAND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_secure_maintenance.h"

typedef enum {
  SECURE_MAINTENANCE_RESULT_INVALID = 0,
  SECURE_MAINTENANCE_RESULT_DENIED,
  SECURE_MAINTENANCE_RESULT_DEBUG_UNLOCKED,
  SECURE_MAINTENANCE_RESULT_UPDATE_AUTHORIZED,
} secure_maintenance_result_t;

typedef struct {
  volatile sec0_handle_t *security;
  uint32_t minimum_version;
  uint32_t debug_sequence;
  uint32_t update_sequence;
  uint32_t challenge;
  uint32_t deadline;
  uint8_t authentication_failures;
  bool initialized;
  bool challenge_active;
  bool debug_unlocked;
  bool update_authorized;
  bool locked_out;
} secure_maintenance_t;

bool secure_maintenance_init(
  secure_maintenance_t *maintenance,
  volatile sec0_handle_t *security,
  uint32_t minimum_version
);
secure_maintenance_result_t secure_maintenance_begin_debug(
  secure_maintenance_t *maintenance,
  uint32_t now,
  uint32_t challenge_ttl
);
secure_maintenance_result_t secure_maintenance_process(
  secure_maintenance_t *maintenance,
  const uint8_t *frame,
  size_t length,
  uint32_t now
);
secure_maintenance_result_t secure_maintenance_expire(
  secure_maintenance_t *maintenance,
  uint32_t now
);

#endif

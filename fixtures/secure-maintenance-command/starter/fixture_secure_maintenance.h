#ifndef FIXTURE_SECURE_MAINTENANCE_H
#define FIXTURE_SECURE_MAINTENANCE_H

#include <stdbool.h>
#include <stdint.h>

#define SEC0_DEBUG_MAGIC UINT32_C(0x30474244)
#define SEC0_UPDATE_MAGIC UINT32_C(0x30504455)
#define SEC0_PROTOCOL_VERSION UINT8_C(1)
#define SEC0_DEBUG_COMMAND UINT8_C(0x31)
#define SEC0_UPDATE_COMMAND UINT8_C(0x41)
#define SEC0_DEBUG_FRAME_BYTES 16u
#define SEC0_UPDATE_FRAME_BYTES 24u
#define SEC0_DEBUG_LOCKOUT_LIMIT UINT8_C(3)
#define SEC0_MAX_CHALLENGE_TTL UINT32_C(0x7fffffff)

#define SEC0_DEBUG_GATE_LOCKED UINT32_C(0)
#define SEC0_DEBUG_GATE_UNLOCKED UINT32_C(1)

typedef enum {
  SEC0_LIFECYCLE_DEVELOPMENT = 0,
  SEC0_LIFECYCLE_PRODUCTION = 1,
  SEC0_LIFECYCLE_RMA = 2,
  SEC0_LIFECYCLE_LOCKED = 3,
} sec0_lifecycle_t;

typedef enum {
  SEC0_SLOT_A = 0,
  SEC0_SLOT_B = 1,
} sec0_slot_t;

typedef struct sec0_handle sec0_handle_t;

sec0_lifecycle_t sec0_read_lifecycle(
  const volatile sec0_handle_t *security
);
bool sec0_read_physical_presence(
  const volatile sec0_handle_t *security
);
uint32_t sec0_issue_challenge(
  volatile sec0_handle_t *security
);
bool sec0_verify_debug_response(
  const volatile sec0_handle_t *security,
  uint32_t sequence,
  uint32_t challenge,
  uint16_t response_tag
);
bool sec0_verify_update_authorization(
  const volatile sec0_handle_t *security,
  uint32_t sequence,
  sec0_slot_t slot,
  uint32_t firmware_version,
  uint32_t image_digest,
  uint16_t signature_tag
);
void sec0_write_debug_gate(
  volatile sec0_handle_t *security,
  uint32_t value
);
void sec0_write_update_gate(
  volatile sec0_handle_t *security,
  uint32_t value
);
void sec0_write_update_authorization(
  volatile sec0_handle_t *security,
  sec0_slot_t slot,
  uint32_t firmware_version,
  uint32_t image_digest
);
void sec0_revoke_update_authorization(
  volatile sec0_handle_t *security
);

#endif

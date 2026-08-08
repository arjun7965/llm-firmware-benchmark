#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "secure_maintenance_command.h"

static uint16_t read_u16(const uint8_t *bytes) {
  return (uint16_t)bytes[0] | (uint16_t)((uint16_t)bytes[1] << 8);
}

static uint32_t read_u32(const uint8_t *bytes) {
  return (uint32_t)bytes[0] |
    ((uint32_t)bytes[1] << 8) |
    ((uint32_t)bytes[2] << 16) |
    ((uint32_t)bytes[3] << 24);
}

static bool ready(const secure_maintenance_t *maintenance) {
  return maintenance != NULL && maintenance->initialized &&
    maintenance->security != NULL;
}

static bool deadline_reached(uint32_t now, uint32_t deadline) {
  return (now - deadline) < UINT32_C(0x80000000);
}

static bool sequence_is_newer(uint32_t last, uint32_t candidate) {
  return candidate != 0u && candidate != UINT32_MAX &&
    last != UINT32_MAX && candidate > last;
}

static secure_maintenance_result_t deny(secure_maintenance_t *maintenance) {
  sec0_write_debug_gate(maintenance->security, SEC0_DEBUG_GATE_LOCKED);
  sec0_write_update_gate(maintenance->security, SEC0_DEBUG_GATE_LOCKED);
  sec0_revoke_update_authorization(maintenance->security);
  maintenance->debug_unlocked = false;
  maintenance->update_authorized = false;
  return SECURE_MAINTENANCE_RESULT_DENIED;
}

static secure_maintenance_result_t authentication_failed(
  secure_maintenance_t *maintenance
) {
  if (maintenance->authentication_failures < SEC0_DEBUG_LOCKOUT_LIMIT) {
    maintenance->authentication_failures++;
  }
  if (maintenance->authentication_failures >= SEC0_DEBUG_LOCKOUT_LIMIT) {
    maintenance->locked_out = true;
    maintenance->challenge_active = false;
  }
  return deny(maintenance);
}

bool secure_maintenance_init(
  secure_maintenance_t *maintenance,
  volatile sec0_handle_t *security,
  uint32_t minimum_version
) {
  if (maintenance == NULL || security == NULL) {
    return false;
  }
  *maintenance = (secure_maintenance_t) {
    .security = security,
    .minimum_version = minimum_version,
    .initialized = true,
  };
  sec0_write_debug_gate(security, SEC0_DEBUG_GATE_LOCKED);
  sec0_write_update_gate(security, SEC0_DEBUG_GATE_LOCKED);
  sec0_revoke_update_authorization(security);
  return true;
}

secure_maintenance_result_t secure_maintenance_begin_debug(
  secure_maintenance_t *maintenance,
  uint32_t now,
  uint32_t challenge_ttl
) {
  sec0_lifecycle_t lifecycle;

  if (!ready(maintenance) || challenge_ttl == 0u ||
    challenge_ttl > SEC0_MAX_CHALLENGE_TTL) {
    return SECURE_MAINTENANCE_RESULT_INVALID;
  }
  lifecycle = sec0_read_lifecycle(maintenance->security);
  if (lifecycle == SEC0_LIFECYCLE_LOCKED ||
    !sec0_read_physical_presence(maintenance->security) ||
    maintenance->locked_out) {
    return deny(maintenance);
  }

  maintenance->challenge = sec0_issue_challenge(maintenance->security);
  maintenance->deadline = now + challenge_ttl;
  maintenance->challenge_active = true;
  maintenance->debug_unlocked = false;
  sec0_write_debug_gate(maintenance->security, SEC0_DEBUG_GATE_LOCKED);
  sec0_write_update_gate(maintenance->security, SEC0_DEBUG_GATE_LOCKED);
  sec0_revoke_update_authorization(maintenance->security);
  maintenance->update_authorized = false;
  return SECURE_MAINTENANCE_RESULT_DENIED;
}

secure_maintenance_result_t secure_maintenance_process(
  secure_maintenance_t *maintenance,
  const uint8_t *frame,
  size_t length,
  uint32_t now
) {
  uint32_t magic;
  uint32_t sequence;

  if (!ready(maintenance) || frame == NULL) {
    return SECURE_MAINTENANCE_RESULT_INVALID;
  }
  if (length == SEC0_DEBUG_FRAME_BYTES) {
    uint32_t challenge;
    uint16_t response_tag;

    magic = read_u32(frame);
    sequence = read_u32(frame + 6);
    challenge = read_u32(frame + 10);
    response_tag = read_u16(frame + 14);
    if (magic != SEC0_DEBUG_MAGIC || frame[4] != SEC0_PROTOCOL_VERSION ||
      frame[5] != SEC0_DEBUG_COMMAND || sequence == 0u ||
      !sequence_is_newer(maintenance->debug_sequence, sequence) ||
      !maintenance->challenge_active || maintenance->locked_out ||
      deadline_reached(now, maintenance->deadline) ||
      challenge != maintenance->challenge) {
      if (maintenance->challenge_active &&
        deadline_reached(now, maintenance->deadline)) {
        maintenance->challenge_active = false;
        maintenance->debug_unlocked = false;
      }
      return deny(maintenance);
    }
    if (!sec0_verify_debug_response(
      maintenance->security, sequence, challenge, response_tag
    )) {
      return authentication_failed(maintenance);
    }
    maintenance->debug_sequence = sequence;
    maintenance->challenge_active = false;
    maintenance->authentication_failures = 0u;
    maintenance->debug_unlocked = true;
    sec0_write_debug_gate(
      maintenance->security,
      SEC0_DEBUG_GATE_UNLOCKED
    );
    return SECURE_MAINTENANCE_RESULT_DEBUG_UNLOCKED;
  }
  if (length == SEC0_UPDATE_FRAME_BYTES) {
    uint32_t firmware_version;
    uint32_t image_digest;
    uint16_t signature_tag;
    sec0_slot_t slot;

    magic = read_u32(frame);
    sequence = read_u32(frame + 6);
    slot = (sec0_slot_t)frame[10];
    firmware_version = read_u32(frame + 14);
    image_digest = read_u32(frame + 18);
    signature_tag = read_u16(frame + 22);
    if (magic != SEC0_UPDATE_MAGIC || frame[4] != SEC0_PROTOCOL_VERSION ||
      frame[5] != SEC0_UPDATE_COMMAND || sequence == 0u ||
      !sequence_is_newer(maintenance->update_sequence, sequence) ||
      (slot != SEC0_SLOT_A && slot != SEC0_SLOT_B) ||
      frame[11] != 0u || frame[12] != 0u || frame[13] != 0u ||
      firmware_version <= maintenance->minimum_version) {
      return deny(maintenance);
    }
    if (!sec0_verify_update_authorization(
      maintenance->security,
      sequence,
      slot,
      firmware_version,
      image_digest,
      signature_tag
    )) {
      return deny(maintenance);
    }
    maintenance->update_sequence = sequence;
    maintenance->update_authorized = true;
    sec0_write_update_authorization(
      maintenance->security, slot, firmware_version, image_digest
    );
    sec0_write_update_gate(
      maintenance->security, SEC0_DEBUG_GATE_UNLOCKED
    );
    return SECURE_MAINTENANCE_RESULT_UPDATE_AUTHORIZED;
  }
  return deny(maintenance);
}

secure_maintenance_result_t secure_maintenance_expire(
  secure_maintenance_t *maintenance,
  uint32_t now
) {
  if (!ready(maintenance)) return SECURE_MAINTENANCE_RESULT_INVALID;
  if (maintenance->challenge_active &&
    deadline_reached(now, maintenance->deadline)) {
    maintenance->challenge_active = false;
    maintenance->debug_unlocked = false;
  }
  return deny(maintenance);
}

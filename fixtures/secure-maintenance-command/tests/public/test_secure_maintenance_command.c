#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "mock_secure_maintenance.h"
#include "secure_maintenance_command.h"

#define CHECK(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
    return false; \
  } \
} while (false)

static void put16(uint8_t *frame, size_t offset, uint16_t value) {
  frame[offset] = (uint8_t)value;
  frame[offset + 1u] = (uint8_t)(value >> 8);
}

static void put32(uint8_t *frame, size_t offset, uint32_t value) {
  frame[offset] = (uint8_t)value;
  frame[offset + 1u] = (uint8_t)(value >> 8);
  frame[offset + 2u] = (uint8_t)(value >> 16);
  frame[offset + 3u] = (uint8_t)(value >> 24);
}

static void debug_frame(uint8_t *frame, uint32_t sequence, uint32_t challenge) {
  put32(frame, 0u, SEC0_DEBUG_MAGIC);
  frame[4] = SEC0_PROTOCOL_VERSION;
  frame[5] = SEC0_DEBUG_COMMAND;
  put32(frame, 6u, sequence);
  put32(frame, 10u, challenge);
  put16(frame, 14u, UINT16_C(0x77aa));
}

static void update_frame(uint8_t *frame, uint32_t sequence) {
  put32(frame, 0u, SEC0_UPDATE_MAGIC);
  frame[4] = SEC0_PROTOCOL_VERSION;
  frame[5] = SEC0_UPDATE_COMMAND;
  put32(frame, 6u, sequence);
  frame[10] = SEC0_SLOT_B;
  frame[11] = 0u;
  frame[12] = 0u;
  frame[13] = 0u;
  put32(frame, 14u, UINT32_C(8));
  put32(frame, 18u, UINT32_C(0x11223344));
  put16(frame, 22u, UINT16_C(0x55aa));
}

static bool initialize(secure_maintenance_t *maintenance) {
  mock_sec0_reset();
  return secure_maintenance_init(maintenance, mock_sec0(), UINT32_C(7));
}

static bool test_exact_frames_and_independent_replay(void) {
  secure_maintenance_t maintenance = { 0 };
  uint8_t debug[SEC0_DEBUG_FRAME_BYTES + 1u] = { 0 };
  uint8_t update[SEC0_UPDATE_FRAME_BYTES] = { 0 };
  size_t before_verify;

  CHECK(initialize(&maintenance));
  CHECK(mock_sec0_debug_gate() == SEC0_DEBUG_GATE_LOCKED);
  CHECK(mock_sec0_update_gate() == SEC0_DEBUG_GATE_LOCKED);
  mock_sec0_set_policy(SEC0_LIFECYCLE_PRODUCTION, true);
  mock_sec0_set_debug_verdict(true);
  CHECK(secure_maintenance_begin_debug(&maintenance, UINT32_C(100), 20u) ==
    SECURE_MAINTENANCE_RESULT_DENIED);
  debug_frame(debug + 1u, 1u, UINT32_C(0x12345678));
  before_verify = mock_sec0_event_count();
  CHECK(secure_maintenance_process(&maintenance, debug + 1u,
    SEC0_DEBUG_FRAME_BYTES, 110u) ==
    SECURE_MAINTENANCE_RESULT_DEBUG_UNLOCKED);
  CHECK(mock_sec0_event_count() == before_verify + 2u);
  CHECK(mock_sec0_event_at(before_verify) == MOCK_SEC0_EVENT_DEBUG_VERIFY);
  CHECK(mock_sec0_event_at(before_verify + 1u) == MOCK_SEC0_EVENT_DEBUG_GATE);
  CHECK(mock_sec0_last_debug_verify_sequence() == 1u);
  CHECK(mock_sec0_last_debug_verify_challenge() == UINT32_C(0x12345678));
  CHECK(mock_sec0_last_debug_verify_tag() == UINT16_C(0x77aa));
  CHECK(maintenance.debug_sequence == 1u);
  CHECK(secure_maintenance_process(&maintenance, debug + 1u,
    SEC0_DEBUG_FRAME_BYTES, 110u) == SECURE_MAINTENANCE_RESULT_DENIED);
  CHECK(maintenance.debug_sequence == 1u);
  debug_frame(debug + 1u, 2u, UINT32_C(0x12345678));
  CHECK(secure_maintenance_process(&maintenance, debug + 1u,
    SEC0_DEBUG_FRAME_BYTES, 110u) == SECURE_MAINTENANCE_RESULT_DENIED);
  CHECK(maintenance.debug_sequence == 1u);
  mock_sec0_set_challenge(UINT32_C(0x87654321));
  CHECK(secure_maintenance_begin_debug(&maintenance, 110u, 20u) ==
    SECURE_MAINTENANCE_RESULT_DENIED);
  debug_frame(debug + 1u, 1u, UINT32_C(0x87654321));
  CHECK(secure_maintenance_process(&maintenance, debug + 1u,
    SEC0_DEBUG_FRAME_BYTES, 111u) == SECURE_MAINTENANCE_RESULT_DENIED);
  CHECK(maintenance.debug_sequence == 1u);
  mock_sec0_set_update_verdict(true);
  update_frame(update, 1u);
  CHECK(secure_maintenance_process(&maintenance, update,
    SEC0_UPDATE_FRAME_BYTES, 110u) ==
    SECURE_MAINTENANCE_RESULT_UPDATE_AUTHORIZED);
  CHECK(maintenance.update_sequence == 1u);
  CHECK(mock_sec0_update_written());
  CHECK(mock_sec0_update_gate() == SEC0_DEBUG_GATE_UNLOCKED);
  CHECK(mock_sec0_last_update_slot() == SEC0_SLOT_B);
  CHECK(mock_sec0_last_update_version() == 8u);
  CHECK(mock_sec0_last_update_digest() == UINT32_C(0x11223344));
  CHECK(mock_sec0_event_at(mock_sec0_event_count() - 2u) ==
    MOCK_SEC0_EVENT_UPDATE_AUTHORIZATION);
  CHECK(mock_sec0_event_at(mock_sec0_event_count() - 1u) ==
    MOCK_SEC0_EVENT_UPDATE_GATE);
  before_verify = mock_sec0_event_count();
  CHECK(secure_maintenance_process(&maintenance, update,
    SEC0_UPDATE_FRAME_BYTES, 110u) == SECURE_MAINTENANCE_RESULT_DENIED);
  CHECK(maintenance.update_sequence == 1u);
  CHECK(!maintenance.update_authorized);
  CHECK(mock_sec0_update_gate() == SEC0_DEBUG_GATE_LOCKED);
  CHECK(!mock_sec0_update_written());
  CHECK(mock_sec0_event_count() == before_verify + 3u);
  CHECK(mock_sec0_event_at(before_verify) == MOCK_SEC0_EVENT_DEBUG_GATE);
  CHECK(mock_sec0_event_at(before_verify + 1u) == MOCK_SEC0_EVENT_UPDATE_GATE);
  CHECK(mock_sec0_event_at(before_verify + 2u) == MOCK_SEC0_EVENT_UPDATE_REVOKE);

  mock_sec0_set_challenge(UINT32_C(0x87654321));
  CHECK(secure_maintenance_begin_debug(&maintenance, 120u, 20u) ==
    SECURE_MAINTENANCE_RESULT_DENIED);
  CHECK(maintenance.challenge_active);
  CHECK(!maintenance.update_authorized);
  CHECK(mock_sec0_debug_gate() == SEC0_DEBUG_GATE_LOCKED);
  CHECK(mock_sec0_update_gate() == SEC0_DEBUG_GATE_LOCKED);
  CHECK(mock_sec0_update_revoked());
  CHECK(mock_sec0_event_at(mock_sec0_event_count() - 3u) ==
    MOCK_SEC0_EVENT_DEBUG_GATE);
  CHECK(mock_sec0_event_at(mock_sec0_event_count() - 2u) ==
    MOCK_SEC0_EVENT_UPDATE_GATE);
  CHECK(mock_sec0_event_at(mock_sec0_event_count() - 1u) ==
    MOCK_SEC0_EVENT_UPDATE_REVOKE);
  CHECK(!mock_sec0_invalid_access());
  return true;
}

static bool test_policy_malformed_and_unaligned_rejection(void) {
  secure_maintenance_t maintenance = { 0 };
  uint8_t frame[SEC0_DEBUG_FRAME_BYTES + 2u] = { 0 };
  size_t before;

  CHECK(initialize(&maintenance));
  mock_sec0_set_policy(SEC0_LIFECYCLE_PRODUCTION, false);
  CHECK(secure_maintenance_begin_debug(&maintenance, 0u, 4u) ==
    SECURE_MAINTENANCE_RESULT_DENIED);
  CHECK(!maintenance.challenge_active);
  CHECK(mock_sec0_debug_gate() == SEC0_DEBUG_GATE_LOCKED);
  CHECK(mock_sec0_update_gate() == SEC0_DEBUG_GATE_LOCKED);
  CHECK(mock_sec0_update_revoked());
  mock_sec0_set_policy(SEC0_LIFECYCLE_LOCKED, true);
  CHECK(secure_maintenance_begin_debug(&maintenance, 0u, 4u) ==
    SECURE_MAINTENANCE_RESULT_DENIED);
  CHECK(!maintenance.challenge_active);
  CHECK(mock_sec0_debug_gate() == SEC0_DEBUG_GATE_LOCKED);
  CHECK(mock_sec0_update_gate() == SEC0_DEBUG_GATE_LOCKED);
  CHECK(mock_sec0_update_revoked());
  before = mock_sec0_event_count();
  debug_frame(frame + 1u, 1u, UINT32_C(0x12345678));
  frame[1] ^= 1u;
  CHECK(secure_maintenance_process(&maintenance, frame + 1u,
    SEC0_DEBUG_FRAME_BYTES - 1u, 1u) ==
    SECURE_MAINTENANCE_RESULT_DENIED);
  CHECK(mock_sec0_event_count() == before + 3u);
  CHECK(mock_sec0_event_at(before) == MOCK_SEC0_EVENT_DEBUG_GATE);
  CHECK(mock_sec0_event_at(before + 1u) == MOCK_SEC0_EVENT_UPDATE_GATE);
  CHECK(mock_sec0_event_at(before + 2u) == MOCK_SEC0_EVENT_UPDATE_REVOKE);
  CHECK(maintenance.authentication_failures == 0u);
  CHECK(secure_maintenance_process(&maintenance, frame + 1u,
    SEC0_DEBUG_FRAME_BYTES, 1u) == SECURE_MAINTENANCE_RESULT_DENIED);
  CHECK(maintenance.authentication_failures == 0u);

  mock_sec0_set_policy(SEC0_LIFECYCLE_PRODUCTION, true);
  mock_sec0_set_debug_verdict(true);
  CHECK(secure_maintenance_begin_debug(&maintenance, 2u, 8u) ==
    SECURE_MAINTENANCE_RESULT_DENIED);
  debug_frame(frame + 1u, 1u, UINT32_C(0x12345678));
  CHECK(secure_maintenance_process(&maintenance, frame + 1u,
    SEC0_DEBUG_FRAME_BYTES + 1u, 3u) == SECURE_MAINTENANCE_RESULT_DENIED);
  CHECK(maintenance.challenge_active);
  frame[1] ^= 1u;
  CHECK(secure_maintenance_process(&maintenance, frame + 1u,
    SEC0_DEBUG_FRAME_BYTES - 1u, 3u) ==
    SECURE_MAINTENANCE_RESULT_DENIED);
  frame[1] ^= 1u;
  frame[5] ^= 1u;
  CHECK(secure_maintenance_process(&maintenance, frame + 1u,
    SEC0_DEBUG_FRAME_BYTES, 3u) == SECURE_MAINTENANCE_RESULT_DENIED);
  CHECK(mock_sec0_event_at(mock_sec0_event_count() - 3u) ==
    MOCK_SEC0_EVENT_DEBUG_GATE);
  CHECK(mock_sec0_event_at(mock_sec0_event_count() - 2u) ==
    MOCK_SEC0_EVENT_UPDATE_GATE);
  CHECK(mock_sec0_event_at(mock_sec0_event_count() - 1u) ==
    MOCK_SEC0_EVENT_UPDATE_REVOKE);
  frame[5] ^= 1u;
  frame[4] ^= 1u;
  CHECK(secure_maintenance_process(&maintenance, frame + 1u,
    SEC0_DEBUG_FRAME_BYTES, 3u) == SECURE_MAINTENANCE_RESULT_DENIED);
  frame[4] ^= 1u;

  {
    uint8_t update[SEC0_UPDATE_FRAME_BYTES] = { 0 };
    mock_sec0_set_update_verdict(true);
    update_frame(update, 1u);
    update[4] ^= 1u;
    CHECK(secure_maintenance_process(&maintenance, update, sizeof(update), 3u) ==
      SECURE_MAINTENANCE_RESULT_DENIED);
    update[4] ^= 1u;
    update[5] ^= 1u;
    CHECK(secure_maintenance_process(&maintenance, update, sizeof(update), 3u) ==
      SECURE_MAINTENANCE_RESULT_DENIED);
    update[5] ^= 1u;
    update[11] = 1u;
    CHECK(secure_maintenance_process(&maintenance, update, sizeof(update), 3u) ==
      SECURE_MAINTENANCE_RESULT_DENIED);
    update[11] = 0u;
    update[10] = UINT8_C(9);
    CHECK(secure_maintenance_process(&maintenance, update, sizeof(update), 3u) ==
      SECURE_MAINTENANCE_RESULT_DENIED);
  }
  CHECK(!mock_sec0_invalid_access());
  return true;
}

static bool test_authentication_failure_lockout_and_sequence_commit(void) {
  secure_maintenance_t maintenance = { 0 };
  uint8_t frame[SEC0_DEBUG_FRAME_BYTES] = { 0 };

  CHECK(initialize(&maintenance));
  mock_sec0_set_policy(SEC0_LIFECYCLE_PRODUCTION, true);
  mock_sec0_set_debug_verdict(false);
  CHECK(secure_maintenance_begin_debug(&maintenance, 10u, 10u) ==
    SECURE_MAINTENANCE_RESULT_DENIED);
  debug_frame(frame, 1u, UINT32_C(0x12345678));
  CHECK(secure_maintenance_process(&maintenance, frame, sizeof(frame), 11u) ==
    SECURE_MAINTENANCE_RESULT_DENIED);
  CHECK(maintenance.debug_sequence == 0u);
  CHECK(maintenance.authentication_failures == 1u);
  CHECK(secure_maintenance_begin_debug(&maintenance, 20u, 10u) ==
    SECURE_MAINTENANCE_RESULT_DENIED);
  CHECK(maintenance.authentication_failures == 1u);
  CHECK(secure_maintenance_process(&maintenance, frame, sizeof(frame), 11u) ==
    SECURE_MAINTENANCE_RESULT_DENIED);
  CHECK(secure_maintenance_process(&maintenance, frame, sizeof(frame), 11u) ==
    SECURE_MAINTENANCE_RESULT_DENIED);
  CHECK(maintenance.locked_out);
  CHECK(!maintenance.challenge_active);
  CHECK(mock_sec0_debug_gate() == SEC0_DEBUG_GATE_LOCKED);
  CHECK(mock_sec0_update_gate() == SEC0_DEBUG_GATE_LOCKED);
  CHECK(mock_sec0_update_revoked());
  return true;
}

static bool test_expiry_and_wrap_safe_deadline(void) {
  secure_maintenance_t maintenance = { 0 };
  uint8_t frame[SEC0_DEBUG_FRAME_BYTES] = { 0 };

  CHECK(initialize(&maintenance));
  mock_sec0_set_policy(SEC0_LIFECYCLE_PRODUCTION, true);
  mock_sec0_set_debug_verdict(true);
  mock_sec0_set_challenge(UINT32_C(0xaabbccdd));
  CHECK(secure_maintenance_begin_debug(&maintenance, UINT32_MAX - 3u, 8u) ==
    SECURE_MAINTENANCE_RESULT_DENIED);
  debug_frame(frame, 1u, UINT32_C(0xaabbccde));
  CHECK(secure_maintenance_process(&maintenance, frame, sizeof(frame), 2u) ==
    SECURE_MAINTENANCE_RESULT_DENIED);
  CHECK(maintenance.challenge_active);
  debug_frame(frame, 1u, UINT32_C(0xaabbccdd));
  CHECK(secure_maintenance_process(&maintenance, frame, sizeof(frame), 4u) ==
    SECURE_MAINTENANCE_RESULT_DENIED);
  CHECK(!maintenance.challenge_active);
  CHECK(secure_maintenance_expire(&maintenance, 4u) ==
    SECURE_MAINTENANCE_RESULT_DENIED);
  CHECK(!maintenance.challenge_active);
  CHECK(mock_sec0_debug_gate() == SEC0_DEBUG_GATE_LOCKED);
  CHECK(mock_sec0_update_gate() == SEC0_DEBUG_GATE_LOCKED);
  return true;
}

static bool test_update_policy_and_invalid_api(void) {
  secure_maintenance_t maintenance = { 0 };
  uint8_t frame[SEC0_UPDATE_FRAME_BYTES] = { 0 };
  const secure_maintenance_t before = maintenance;

  mock_sec0_reset();
  CHECK(!secure_maintenance_init(NULL, mock_sec0(), 1u));
  CHECK(secure_maintenance_init(&maintenance, mock_sec0(), 0u));
  CHECK(secure_maintenance_process(NULL, frame, sizeof(frame), 0u) ==
    SECURE_MAINTENANCE_RESULT_INVALID);
  CHECK(maintenance.initialized != before.initialized);
  CHECK(initialize(&maintenance));
  mock_sec0_set_update_verdict(false);
  update_frame(frame, 1u);
  CHECK(secure_maintenance_process(&maintenance, frame, sizeof(frame), 0u) ==
    SECURE_MAINTENANCE_RESULT_DENIED);
  CHECK(!maintenance.update_authorized);
  CHECK(maintenance.update_sequence == 0u);
  CHECK(mock_sec0_update_gate() == SEC0_DEBUG_GATE_LOCKED);
  CHECK(mock_sec0_update_revoked());
  CHECK(mock_sec0_last_update_verify_sequence() == 1u);
  CHECK(mock_sec0_last_update_verify_slot() == SEC0_SLOT_B);
  CHECK(mock_sec0_last_update_verify_version() == 8u);
  CHECK(mock_sec0_last_update_verify_digest() == UINT32_C(0x11223344));
  CHECK(mock_sec0_last_update_verify_tag() == UINT16_C(0x55aa));
  for (size_t index = 0u; index < mock_sec0_event_count(); index++) {
    CHECK(mock_sec0_event_at(index) != MOCK_SEC0_EVENT_UPDATE_AUTHORIZATION);
  }
  mock_sec0_set_update_verdict(true);
  update_frame(frame, 1u);
  put32(frame, 14u, UINT32_C(7));
  CHECK(secure_maintenance_process(&maintenance, frame, sizeof(frame), 0u) ==
    SECURE_MAINTENANCE_RESULT_DENIED);
  CHECK(!mock_sec0_update_written());
  put32(frame, 14u, UINT32_C(8));
  put32(frame, 6u, 0u);
  CHECK(secure_maintenance_process(&maintenance, frame, sizeof(frame), 0u) ==
    SECURE_MAINTENANCE_RESULT_DENIED);
  put32(frame, 6u, UINT32_MAX);
  CHECK(secure_maintenance_process(&maintenance, frame, sizeof(frame), 0u) ==
    SECURE_MAINTENANCE_RESULT_DENIED);
  frame[11] = 1u;
  put32(frame, 14u, UINT32_C(8));
  CHECK(secure_maintenance_process(&maintenance, frame, sizeof(frame), 0u) ==
    SECURE_MAINTENANCE_RESULT_DENIED);
  CHECK(!mock_sec0_update_written());
  return true;
}

int main(void) {
  const struct { const char *name; bool (*run)(void); } tests[] = {
    { "exact frames and replay", test_exact_frames_and_independent_replay },
    { "policy and malformed input", test_policy_malformed_and_unaligned_rejection },
    { "authentication lockout", test_authentication_failure_lockout_and_sequence_commit },
    { "deadline expiry", test_expiry_and_wrap_safe_deadline },
    { "update and invalid API", test_update_policy_and_invalid_api },
  };
  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) {
      fprintf(stderr, "failed: %s\n", tests[index].name);
      return 1;
    }
  }
  printf("Secure maintenance public tests passed (%zu tests).\n",
    sizeof(tests) / sizeof(tests[0]));
  return 0;
}

#include <stddef.h>
#include <stdint.h>

#include "mock_secure_maintenance.h"

struct sec0_handle {
  uint32_t marker;
};

typedef struct {
  mock_sec0_event_t type;
  uint32_t value;
} event_t;

static struct sec0_handle handle = { UINT32_C(0x53454330) };
static event_t events[64];
static size_t event_count;
static sec0_lifecycle_t lifecycle;
static bool physical;
static uint32_t challenge = UINT32_C(0x12345678);
static bool debug_verdict;
static bool update_verdict;
static uint32_t debug_gate;
static uint32_t update_gate;
static bool update_written;
static bool update_revoked;
static sec0_slot_t last_update_slot;
static uint32_t last_update_version;
static uint32_t last_update_digest;
static uint32_t last_update_verify_sequence;
static sec0_slot_t last_update_verify_slot;
static uint32_t last_update_verify_version;
static uint32_t last_update_verify_digest;
static uint16_t last_update_verify_tag;
static uint32_t last_debug_verify_sequence;
static uint32_t last_debug_verify_challenge;
static uint16_t last_debug_verify_tag;
static mock_sec0_state_validator_t first_access_validator;
static const void *first_access_context;
static bool first_access_validated;
static bool invalid_access;

static bool good_security(const volatile sec0_handle_t *security) {
  if (security == &handle && handle.marker == UINT32_C(0x53454330)) {
    return true;
  }
  invalid_access = true;
  return false;
}

static void record(mock_sec0_event_t type, uint32_t value) {
  if (!first_access_validated && first_access_validator != NULL) {
    first_access_validated = true;
    if (!first_access_validator(first_access_context)) invalid_access = true;
  }
  if (event_count < sizeof(events) / sizeof(events[0])) {
    events[event_count++] = (event_t) { type, value };
  }
}

volatile sec0_handle_t *mock_sec0(void) { return &handle; }

void mock_sec0_reset(void) {
  event_count = 0u;
  lifecycle = SEC0_LIFECYCLE_PRODUCTION;
  physical = false;
  challenge = UINT32_C(0x12345678);
  debug_verdict = false;
  update_verdict = false;
  debug_gate = SEC0_DEBUG_GATE_UNLOCKED;
  update_gate = SEC0_DEBUG_GATE_UNLOCKED;
  update_written = false;
  update_revoked = false;
  last_update_slot = SEC0_SLOT_A;
  last_update_version = 0u;
  last_update_digest = 0u;
  last_update_verify_sequence = 0u;
  last_update_verify_slot = SEC0_SLOT_A;
  last_update_verify_version = 0u;
  last_update_verify_digest = 0u;
  last_update_verify_tag = 0u;
  last_debug_verify_sequence = 0u;
  last_debug_verify_challenge = 0u;
  last_debug_verify_tag = 0u;
  first_access_validator = NULL;
  first_access_context = NULL;
  first_access_validated = false;
  invalid_access = false;
}

void mock_sec0_set_first_access_validator(
  mock_sec0_state_validator_t validator,
  const void *context
) {
  first_access_validator = validator;
  first_access_context = context;
  first_access_validated = false;
}
bool mock_sec0_first_access_validated(void) { return first_access_validated; }
void mock_sec0_set_policy(sec0_lifecycle_t value, bool present) {
  lifecycle = value;
  physical = present;
}

void mock_sec0_set_challenge(uint32_t value) { challenge = value; }
void mock_sec0_set_debug_verdict(bool value) { debug_verdict = value; }
void mock_sec0_set_update_verdict(bool value) { update_verdict = value; }
size_t mock_sec0_event_count(void) { return event_count; }
mock_sec0_event_t mock_sec0_event_at(size_t index) { return events[index].type; }
uint32_t mock_sec0_event_value(size_t index) { return events[index].value; }
uint32_t mock_sec0_debug_gate(void) { return debug_gate; }
uint32_t mock_sec0_update_gate(void) { return update_gate; }
bool mock_sec0_update_written(void) { return update_written; }
bool mock_sec0_update_revoked(void) { return update_revoked; }
sec0_slot_t mock_sec0_last_update_slot(void) { return last_update_slot; }
uint32_t mock_sec0_last_update_version(void) { return last_update_version; }
uint32_t mock_sec0_last_update_digest(void) { return last_update_digest; }
uint32_t mock_sec0_last_update_verify_sequence(void) { return last_update_verify_sequence; }
sec0_slot_t mock_sec0_last_update_verify_slot(void) { return last_update_verify_slot; }
uint32_t mock_sec0_last_update_verify_version(void) { return last_update_verify_version; }
uint32_t mock_sec0_last_update_verify_digest(void) { return last_update_verify_digest; }
uint16_t mock_sec0_last_update_verify_tag(void) { return last_update_verify_tag; }
uint32_t mock_sec0_last_debug_verify_sequence(void) { return last_debug_verify_sequence; }
uint32_t mock_sec0_last_debug_verify_challenge(void) { return last_debug_verify_challenge; }
uint16_t mock_sec0_last_debug_verify_tag(void) { return last_debug_verify_tag; }
bool mock_sec0_invalid_access(void) {
  return invalid_access || handle.marker != UINT32_C(0x53454330);
}

sec0_lifecycle_t sec0_read_lifecycle(const volatile sec0_handle_t *security) {
  if (!good_security(security)) return SEC0_LIFECYCLE_DEVELOPMENT;
  record(MOCK_SEC0_EVENT_LIFECYCLE, (uint32_t)lifecycle);
  return lifecycle;
}

bool sec0_read_physical_presence(const volatile sec0_handle_t *security) {
  if (!good_security(security)) return false;
  record(MOCK_SEC0_EVENT_PHYSICAL, physical ? 1u : 0u);
  return physical;
}

uint32_t sec0_issue_challenge(volatile sec0_handle_t *security) {
  if (!good_security(security)) return 0u;
  record(MOCK_SEC0_EVENT_CHALLENGE, challenge);
  return challenge;
}

bool sec0_verify_debug_response(
  const volatile sec0_handle_t *security,
  uint32_t sequence,
  uint32_t response_challenge,
  uint16_t response_tag
) {
  if (!good_security(security)) return false;
  last_debug_verify_sequence = sequence;
  last_debug_verify_challenge = response_challenge;
  last_debug_verify_tag = response_tag;
  record(MOCK_SEC0_EVENT_DEBUG_VERIFY, sequence);
  return debug_verdict;
}

bool sec0_verify_update_authorization(
  const volatile sec0_handle_t *security,
  uint32_t sequence,
  sec0_slot_t slot,
  uint32_t firmware_version,
  uint32_t image_digest,
  uint16_t signature_tag
) {
  if (!good_security(security)) return false;
  last_update_verify_sequence = sequence;
  last_update_verify_slot = slot;
  last_update_verify_version = firmware_version;
  last_update_verify_digest = image_digest;
  last_update_verify_tag = signature_tag;
  record(MOCK_SEC0_EVENT_UPDATE_VERIFY, sequence);
  return update_verdict;
}

void sec0_write_debug_gate(volatile sec0_handle_t *security, uint32_t value) {
  if (!good_security(security)) return;
  debug_gate = value;
  record(MOCK_SEC0_EVENT_DEBUG_GATE, value);
}

void sec0_write_update_gate(volatile sec0_handle_t *security, uint32_t value) {
  if (!good_security(security)) return;
  update_gate = value;
  record(MOCK_SEC0_EVENT_UPDATE_GATE, value);
}

void sec0_write_update_authorization(
  volatile sec0_handle_t *security,
  sec0_slot_t slot,
  uint32_t firmware_version,
  uint32_t image_digest
) {
  if (!good_security(security)) return;
  update_written = true;
  last_update_slot = slot;
  last_update_version = firmware_version;
  last_update_digest = image_digest;
  record(MOCK_SEC0_EVENT_UPDATE_AUTHORIZATION, firmware_version);
}

void sec0_revoke_update_authorization(volatile sec0_handle_t *security) {
  if (!good_security(security)) return;
  update_written = false;
  update_revoked = true;
  last_update_slot = SEC0_SLOT_A;
  last_update_version = 0u;
  last_update_digest = 0u;
  record(MOCK_SEC0_EVENT_UPDATE_REVOKE, 0u);
}

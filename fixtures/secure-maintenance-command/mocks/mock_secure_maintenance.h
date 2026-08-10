#ifndef MOCK_SECURE_MAINTENANCE_H
#define MOCK_SECURE_MAINTENANCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_secure_maintenance.h"

typedef enum {
  MOCK_SEC0_EVENT_LIFECYCLE = 0,
  MOCK_SEC0_EVENT_PHYSICAL,
  MOCK_SEC0_EVENT_CHALLENGE,
  MOCK_SEC0_EVENT_DEBUG_VERIFY,
  MOCK_SEC0_EVENT_UPDATE_VERIFY,
  MOCK_SEC0_EVENT_DEBUG_GATE,
  MOCK_SEC0_EVENT_UPDATE_GATE,
  MOCK_SEC0_EVENT_UPDATE_AUTHORIZATION,
  MOCK_SEC0_EVENT_UPDATE_REVOKE,
} mock_sec0_event_t;

typedef bool (*mock_sec0_state_validator_t)(const void *context);

volatile sec0_handle_t *mock_sec0(void);
void mock_sec0_reset(void);
void mock_sec0_set_first_access_validator(
  mock_sec0_state_validator_t validator,
  const void *context
);
bool mock_sec0_first_access_validated(void);
void mock_sec0_set_policy(sec0_lifecycle_t lifecycle, bool physical);
void mock_sec0_set_challenge(uint32_t challenge);
void mock_sec0_set_debug_verdict(bool verdict);
void mock_sec0_set_update_verdict(bool verdict);
size_t mock_sec0_event_count(void);
mock_sec0_event_t mock_sec0_event_at(size_t index);
uint32_t mock_sec0_event_value(size_t index);
uint32_t mock_sec0_debug_gate(void);
uint32_t mock_sec0_update_gate(void);
bool mock_sec0_update_written(void);
bool mock_sec0_update_revoked(void);
sec0_slot_t mock_sec0_last_update_slot(void);
uint32_t mock_sec0_last_update_version(void);
uint32_t mock_sec0_last_update_digest(void);
uint32_t mock_sec0_last_update_verify_sequence(void);
sec0_slot_t mock_sec0_last_update_verify_slot(void);
uint32_t mock_sec0_last_update_verify_version(void);
uint32_t mock_sec0_last_update_verify_digest(void);
uint16_t mock_sec0_last_update_verify_tag(void);
uint32_t mock_sec0_last_debug_verify_sequence(void);
uint32_t mock_sec0_last_debug_verify_challenge(void);
uint16_t mock_sec0_last_debug_verify_tag(void);
bool mock_sec0_invalid_access(void);

#endif

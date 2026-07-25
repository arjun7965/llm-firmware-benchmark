#ifndef MOCK_SECURE_BOOT_IMAGE_H
#define MOCK_SECURE_BOOT_IMAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_secure_boot_image.h"

typedef enum {
  MOCK_BOOT0_EVENT_HEADER_READ,
  MOCK_BOOT0_EVENT_IMAGE_MEASURE,
  MOCK_BOOT0_EVENT_SIGNATURE_VERIFY,
  MOCK_BOOT0_EVENT_BOOT_SLOT_WRITE,
  MOCK_BOOT0_EVENT_RECOVERY_LOCK_WRITE,
  MOCK_BOOT0_EVENT_COUNT,
} mock_boot0_event_t;

void mock_boot0_reset(void);
volatile boot0_registers_t *mock_boot0(void);
void mock_boot0_set_header(
  boot0_slot_t slot,
  bool present,
  const boot_image_header_t *header
);
void mock_boot0_set_measured_digest(boot0_slot_t slot, uint32_t digest);
void mock_boot0_set_signature_valid(boot0_slot_t slot, bool valid);

boot0_slot_t mock_boot0_boot_slot(void);
uint32_t mock_boot0_recovery_lock(void);
size_t mock_boot0_event_count(void);
mock_boot0_event_t mock_boot0_event_at(size_t index);
uint32_t mock_boot0_event_value(size_t index);
bool mock_boot0_invalid_access(void);

#endif

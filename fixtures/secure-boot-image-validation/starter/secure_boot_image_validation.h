#ifndef SECURE_BOOT_IMAGE_VALIDATION_H
#define SECURE_BOOT_IMAGE_VALIDATION_H

#include <stdbool.h>
#include <stdint.h>

#include "fixture_secure_boot_image.h"

typedef enum {
  SECURE_BOOT_RESULT_INVALID = 0,
  SECURE_BOOT_RESULT_REJECTED_FORMAT,
  SECURE_BOOT_RESULT_REJECTED_VERSION,
  SECURE_BOOT_RESULT_REJECTED_DIGEST,
  SECURE_BOOT_RESULT_REJECTED_SIGNATURE,
  SECURE_BOOT_RESULT_BOOTED,
} secure_boot_result_t;

typedef struct {
  volatile boot0_registers_t *boot;
  uint32_t minimum_version;
  secure_boot_result_t result;
  bool initialized;
} secure_boot_t;

bool secure_boot_init(
  secure_boot_t *boot,
  volatile boot0_registers_t *registers,
  uint32_t minimum_version
);
secure_boot_result_t secure_boot_attempt(
  secure_boot_t *boot,
  boot0_slot_t slot
);

#endif

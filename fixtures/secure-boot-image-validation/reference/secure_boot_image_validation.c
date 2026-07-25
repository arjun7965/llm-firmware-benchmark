#include <stddef.h>

#include "secure_boot_image_validation.h"

static bool slot_is_valid(boot0_slot_t slot) {
  return slot == BOOT0_SLOT_A || slot == BOOT0_SLOT_B;
}

static bool header_is_valid(const boot_image_header_t *header) {
  uint32_t execution_address;

  if (
    header->magic != SECURE_BOOT_IMAGE_MAGIC ||
    header->format_version != SECURE_BOOT_FORMAT_VERSION ||
    header->header_bytes != SECURE_BOOT_HEADER_BYTES ||
    header->firmware_version == 0u ||
    header->firmware_version > SECURE_BOOT_MAX_VERSION
  ) {
    return false;
  }
  if (
    header->image_bytes < SECURE_BOOT_MIN_IMAGE_BYTES ||
    header->image_bytes > SECURE_BOOT_MAX_IMAGE_BYTES ||
    (header->image_bytes & UINT32_C(3)) != 0u ||
    (header->load_address % SECURE_BOOT_LOAD_ALIGNMENT) != 0u
  ) {
    return false;
  }
  if (
    header->load_address < SECURE_BOOT_FLASH_START ||
    header->load_address >= SECURE_BOOT_FLASH_END_EXCLUSIVE ||
    header->image_bytes >
      SECURE_BOOT_FLASH_END_EXCLUSIVE - header->load_address ||
    (header->entry_address & UINT32_C(1)) == 0u
  ) {
    return false;
  }

  execution_address = header->entry_address & ~UINT32_C(1);
  return execution_address >= header->load_address &&
    execution_address - header->load_address < header->image_bytes;
}

static bool boot_is_ready(const secure_boot_t *boot) {
  return boot != NULL && boot->initialized && boot->boot != NULL;
}

static secure_boot_result_t reject(
  secure_boot_t *boot,
  secure_boot_result_t result
) {
  boot0_write_recovery_lock(boot->boot, BOOT0_RECOVERY_LOCKED);
  boot->result = result;
  return result;
}

bool secure_boot_init(
  secure_boot_t *boot,
  volatile boot0_registers_t *registers,
  uint32_t minimum_version
) {
  if (
    boot == NULL || registers == NULL || minimum_version == 0u ||
    minimum_version > SECURE_BOOT_MAX_VERSION
  ) {
    return false;
  }

  *boot = (secure_boot_t) {
    .boot = registers,
    .minimum_version = minimum_version,
    .result = SECURE_BOOT_RESULT_INVALID,
    .initialized = true,
  };
  return true;
}

secure_boot_result_t secure_boot_attempt(
  secure_boot_t *boot,
  boot0_slot_t slot
) {
  boot_image_header_t header;
  uint32_t measured_digest;

  if (!boot_is_ready(boot) || !slot_is_valid(slot)) {
    return SECURE_BOOT_RESULT_INVALID;
  }
  if (!boot0_read_header(boot->boot, slot, &header) ||
    !header_is_valid(&header)) {
    return reject(boot, SECURE_BOOT_RESULT_REJECTED_FORMAT);
  }
  if (header.firmware_version < boot->minimum_version) {
    return reject(boot, SECURE_BOOT_RESULT_REJECTED_VERSION);
  }

  measured_digest = boot0_measure_image(boot->boot, slot);
  if (measured_digest != header.image_digest) {
    return reject(boot, SECURE_BOOT_RESULT_REJECTED_DIGEST);
  }
  if (!boot0_verify_signature(
    boot->boot,
    slot,
    measured_digest,
    header.signature_tag
  )) {
    return reject(boot, SECURE_BOOT_RESULT_REJECTED_SIGNATURE);
  }

  boot0_write_boot_slot(boot->boot, slot);
  boot0_write_recovery_lock(boot->boot, BOOT0_RECOVERY_UNLOCKED);
  boot->result = SECURE_BOOT_RESULT_BOOTED;
  return SECURE_BOOT_RESULT_BOOTED;
}

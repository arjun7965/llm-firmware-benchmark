#ifndef FIXTURE_SECURE_BOOT_IMAGE_H
#define FIXTURE_SECURE_BOOT_IMAGE_H

#include <stdbool.h>
#include <stdint.h>

#define SECURE_BOOT_IMAGE_MAGIC UINT32_C(0x53424F54)
#define SECURE_BOOT_FORMAT_VERSION UINT16_C(1)
#define SECURE_BOOT_HEADER_BYTES UINT16_C(32)
#define SECURE_BOOT_FLASH_START UINT32_C(0x08004000)
#define SECURE_BOOT_FLASH_END_EXCLUSIVE UINT32_C(0x08040000)
#define SECURE_BOOT_LOAD_ALIGNMENT UINT32_C(256)
#define SECURE_BOOT_MIN_IMAGE_BYTES UINT32_C(256)
#define SECURE_BOOT_MAX_IMAGE_BYTES UINT32_C(65536)
#define SECURE_BOOT_MAX_VERSION UINT32_C(4095)

#define BOOT0_RECOVERY_UNLOCKED UINT32_C(0)
#define BOOT0_RECOVERY_LOCKED UINT32_C(1)

typedef enum {
  BOOT0_SLOT_A = 0,
  BOOT0_SLOT_B = 1,
  BOOT0_SLOT_NONE = 2,
} boot0_slot_t;

typedef struct {
  uint32_t magic;
  uint16_t format_version;
  uint16_t header_bytes;
  uint32_t image_bytes;
  uint32_t load_address;
  uint32_t entry_address;
  uint32_t firmware_version;
  uint32_t image_digest;
  uint32_t signature_tag;
} boot_image_header_t;

typedef struct boot0_registers boot0_registers_t;

bool boot0_read_header(
  const volatile boot0_registers_t *boot,
  boot0_slot_t slot,
  boot_image_header_t *header
);
uint32_t boot0_measure_image(
  const volatile boot0_registers_t *boot,
  boot0_slot_t slot
);
bool boot0_verify_signature(
  const volatile boot0_registers_t *boot,
  boot0_slot_t slot,
  uint32_t measured_digest,
  uint32_t signature_tag
);
void boot0_write_boot_slot(
  volatile boot0_registers_t *boot,
  boot0_slot_t slot
);
void boot0_write_recovery_lock(
  volatile boot0_registers_t *boot,
  uint32_t value
);

#endif

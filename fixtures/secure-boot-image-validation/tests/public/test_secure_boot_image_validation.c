#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "mock_secure_boot_image.h"
#include "secure_boot_image_validation.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
        __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

typedef struct {
  mock_boot0_event_t event;
  uint32_t value;
} expected_event_t;

static boot_image_header_t valid_header(void) {
  return (boot_image_header_t) {
    .magic = SECURE_BOOT_IMAGE_MAGIC,
    .format_version = SECURE_BOOT_FORMAT_VERSION,
    .header_bytes = SECURE_BOOT_HEADER_BYTES,
    .image_bytes = UINT32_C(512),
    .load_address = SECURE_BOOT_FLASH_START,
    .entry_address = SECURE_BOOT_FLASH_START + UINT32_C(1),
    .firmware_version = UINT32_C(7),
    .image_digest = UINT32_C(0xC0DEF00D),
    .signature_tag = UINT32_C(0x51A6A7E),
  };
}

static bool boot_equals(const secure_boot_t *left, const secure_boot_t *right) {
  return left->boot == right->boot &&
    left->minimum_version == right->minimum_version &&
    left->result == right->result &&
    left->initialized == right->initialized;
}

static bool events_match_from(
  size_t offset,
  const expected_event_t *expected,
  size_t expected_count
) {
  if (mock_boot0_event_count() != offset + expected_count) return false;
  for (size_t index = 0u; index < expected_count; index++) {
    if (
      mock_boot0_event_at(offset + index) != expected[index].event ||
      mock_boot0_event_value(offset + index) != expected[index].value
    ) {
      return false;
    }
  }
  return true;
}

static bool initialize(secure_boot_t *boot) {
  return secure_boot_init(boot, mock_boot0(), UINT32_C(7));
}

static bool test_initialization_and_verified_transfer(void) {
  secure_boot_t boot = {
    .boot = (volatile boot0_registers_t *)(uintptr_t)UINT32_C(1),
    .minimum_version = UINT32_C(1),
    .result = SECURE_BOOT_RESULT_BOOTED,
    .initialized = true,
  };
  const secure_boot_t before = boot;
  const boot_image_header_t header = valid_header();
  const expected_event_t expected[] = {
    { MOCK_BOOT0_EVENT_HEADER_READ, BOOT0_SLOT_A },
    { MOCK_BOOT0_EVENT_IMAGE_MEASURE, BOOT0_SLOT_A },
    { MOCK_BOOT0_EVENT_SIGNATURE_VERIFY, BOOT0_SLOT_A },
    { MOCK_BOOT0_EVENT_BOOT_SLOT_WRITE, BOOT0_SLOT_A },
    { MOCK_BOOT0_EVENT_RECOVERY_LOCK_WRITE, BOOT0_RECOVERY_UNLOCKED },
  };

  mock_boot0_reset();
  CHECK(!secure_boot_init(NULL, mock_boot0(), UINT32_C(7)));
  CHECK(!secure_boot_init(&boot, NULL, UINT32_C(7)));
  CHECK(!secure_boot_init(&boot, mock_boot0(), 0u));
  CHECK(!secure_boot_init(
    &boot,
    mock_boot0(),
    SECURE_BOOT_MAX_VERSION + UINT32_C(1)
  ));
  CHECK(boot_equals(&boot, &before));
  CHECK(mock_boot0_event_count() == 0u);

  mock_boot0_set_header(BOOT0_SLOT_A, true, &header);
  mock_boot0_set_measured_digest(BOOT0_SLOT_A, header.image_digest);
  mock_boot0_set_signature_valid(BOOT0_SLOT_A, true);
  CHECK(initialize(&boot));
  CHECK(secure_boot_attempt(&boot, BOOT0_SLOT_A) ==
    SECURE_BOOT_RESULT_BOOTED);
  CHECK(events_match_from(
    0u,
    expected,
    sizeof(expected) / sizeof(expected[0])
  ));
  CHECK(boot.result == SECURE_BOOT_RESULT_BOOTED);
  CHECK(mock_boot0_boot_slot() == BOOT0_SLOT_A);
  CHECK(mock_boot0_recovery_lock() == BOOT0_RECOVERY_UNLOCKED);
  CHECK(!mock_boot0_invalid_access());
  return true;
}

static bool test_structure_and_version_rejection_precede_measurement(void) {
  secure_boot_t boot = { 0 };
  boot_image_header_t header = valid_header();
  const expected_event_t rejected[] = {
    { MOCK_BOOT0_EVENT_HEADER_READ, BOOT0_SLOT_B },
    { MOCK_BOOT0_EVENT_RECOVERY_LOCK_WRITE, BOOT0_RECOVERY_LOCKED },
  };
  size_t offset;

  mock_boot0_reset();
  CHECK(initialize(&boot));

  header.magic ^= UINT32_C(1);
  mock_boot0_set_header(BOOT0_SLOT_B, true, &header);
  CHECK(secure_boot_attempt(&boot, BOOT0_SLOT_B) ==
    SECURE_BOOT_RESULT_REJECTED_FORMAT);
  CHECK(events_match_from(
    0u,
    rejected,
    sizeof(rejected) / sizeof(rejected[0])
  ));

  header = valid_header();
  header.firmware_version = UINT32_C(6);
  mock_boot0_set_header(BOOT0_SLOT_B, true, &header);
  offset = mock_boot0_event_count();
  CHECK(secure_boot_attempt(&boot, BOOT0_SLOT_B) ==
    SECURE_BOOT_RESULT_REJECTED_VERSION);
  CHECK(events_match_from(
    offset,
    rejected,
    sizeof(rejected) / sizeof(rejected[0])
  ));

  header = valid_header();
  header.entry_address = header.load_address;
  mock_boot0_set_header(BOOT0_SLOT_B, true, &header);
  offset = mock_boot0_event_count();
  CHECK(secure_boot_attempt(&boot, BOOT0_SLOT_B) ==
    SECURE_BOOT_RESULT_REJECTED_FORMAT);
  CHECK(events_match_from(
    offset,
    rejected,
    sizeof(rejected) / sizeof(rejected[0])
  ));
  CHECK(mock_boot0_boot_slot() == BOOT0_SLOT_NONE);
  CHECK(mock_boot0_recovery_lock() == BOOT0_RECOVERY_LOCKED);
  CHECK(!mock_boot0_invalid_access());
  return true;
}

static bool test_digest_and_signature_gate_boot_selection(void) {
  secure_boot_t boot = { 0 };
  const boot_image_header_t header = valid_header();
  const expected_event_t digest_rejected[] = {
    { MOCK_BOOT0_EVENT_HEADER_READ, BOOT0_SLOT_A },
    { MOCK_BOOT0_EVENT_IMAGE_MEASURE, BOOT0_SLOT_A },
    { MOCK_BOOT0_EVENT_RECOVERY_LOCK_WRITE, BOOT0_RECOVERY_LOCKED },
  };
  const expected_event_t signature_rejected[] = {
    { MOCK_BOOT0_EVENT_HEADER_READ, BOOT0_SLOT_A },
    { MOCK_BOOT0_EVENT_IMAGE_MEASURE, BOOT0_SLOT_A },
    { MOCK_BOOT0_EVENT_SIGNATURE_VERIFY, BOOT0_SLOT_A },
    { MOCK_BOOT0_EVENT_RECOVERY_LOCK_WRITE, BOOT0_RECOVERY_LOCKED },
  };
  size_t offset;

  mock_boot0_reset();
  mock_boot0_set_header(BOOT0_SLOT_A, true, &header);
  mock_boot0_set_measured_digest(
    BOOT0_SLOT_A,
    header.image_digest ^ UINT32_C(1)
  );
  mock_boot0_set_signature_valid(BOOT0_SLOT_A, true);
  CHECK(initialize(&boot));
  CHECK(secure_boot_attempt(&boot, BOOT0_SLOT_A) ==
    SECURE_BOOT_RESULT_REJECTED_DIGEST);
  CHECK(events_match_from(
    0u,
    digest_rejected,
    sizeof(digest_rejected) / sizeof(digest_rejected[0])
  ));

  mock_boot0_set_measured_digest(BOOT0_SLOT_A, header.image_digest);
  mock_boot0_set_signature_valid(BOOT0_SLOT_A, false);
  offset = mock_boot0_event_count();
  CHECK(secure_boot_attempt(&boot, BOOT0_SLOT_A) ==
    SECURE_BOOT_RESULT_REJECTED_SIGNATURE);
  CHECK(events_match_from(
    offset,
    signature_rejected,
    sizeof(signature_rejected) / sizeof(signature_rejected[0])
  ));
  CHECK(mock_boot0_boot_slot() == BOOT0_SLOT_NONE);
  CHECK(mock_boot0_recovery_lock() == BOOT0_RECOVERY_LOCKED);
  CHECK(!mock_boot0_invalid_access());
  return true;
}

static bool test_invalid_attempts_have_no_effect(void) {
  secure_boot_t boot = { 0 };
  const secure_boot_t before = boot;

  mock_boot0_reset();
  CHECK(secure_boot_attempt(NULL, BOOT0_SLOT_A) == SECURE_BOOT_RESULT_INVALID);
  CHECK(secure_boot_attempt(&boot, BOOT0_SLOT_A) ==
    SECURE_BOOT_RESULT_INVALID);
  CHECK(initialize(&boot));
  CHECK(secure_boot_attempt(&boot, BOOT0_SLOT_NONE) ==
    SECURE_BOOT_RESULT_INVALID);
  CHECK(mock_boot0_event_count() == 0u);
  CHECK(boot.initialized);
  CHECK(boot.result == SECURE_BOOT_RESULT_INVALID);
  CHECK(!before.initialized);
  CHECK(!mock_boot0_invalid_access());
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "initialization and verified transfer", test_initialization_and_verified_transfer },
    { "structure and version rejection", test_structure_and_version_rejection_precede_measurement },
    { "digest and signature gate", test_digest_and_signature_gate_boot_selection },
    { "invalid attempts", test_invalid_attempts_have_no_effect },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) {
      fprintf(stderr, "failed: %s\n", tests[index].name);
      return 1;
    }
  }

  printf("Secure boot image-validation public tests passed (%zu tests).\n",
    sizeof(tests) / sizeof(tests[0]));
  return 0;
}

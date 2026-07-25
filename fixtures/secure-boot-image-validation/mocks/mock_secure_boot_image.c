#include "mock_secure_boot_image.h"

#define MOCK_BOOT0_HISTORY_CAPACITY 128u

struct boot0_registers {
  uint32_t reserved;
};

typedef struct {
  mock_boot0_event_t event;
  uint32_t value;
} mock_boot0_event_record_t;

typedef struct {
  struct boot0_registers boot;
  boot_image_header_t headers[2];
  uint32_t measured_digests[2];
  bool header_present[2];
  bool signature_valid[2];
  boot0_slot_t boot_slot;
  uint32_t recovery_lock;
  mock_boot0_event_record_t events[MOCK_BOOT0_HISTORY_CAPACITY];
  size_t event_count;
  bool invalid_access;
} mock_boot0_state_t;

static mock_boot0_state_t state;

static bool slot_is_valid(boot0_slot_t slot) {
  return slot == BOOT0_SLOT_A || slot == BOOT0_SLOT_B;
}

static bool is_boot(const volatile boot0_registers_t *boot) {
  return boot == &state.boot;
}

static void record_event(mock_boot0_event_t event, uint32_t value) {
  if (state.event_count < MOCK_BOOT0_HISTORY_CAPACITY) {
    state.events[state.event_count] = (mock_boot0_event_record_t) {
      .event = event,
      .value = value,
    };
  } else {
    state.invalid_access = true;
  }
  state.event_count++;
}

void mock_boot0_reset(void) {
  state = (mock_boot0_state_t) {
    .boot_slot = BOOT0_SLOT_NONE,
    .recovery_lock = BOOT0_RECOVERY_LOCKED,
  };
}

volatile boot0_registers_t *mock_boot0(void) {
  return &state.boot;
}

void mock_boot0_set_header(
  boot0_slot_t slot,
  bool present,
  const boot_image_header_t *header
) {
  if (!slot_is_valid(slot) || (present && header == NULL)) {
    state.invalid_access = true;
    return;
  }
  state.header_present[slot] = present;
  if (present) state.headers[slot] = *header;
}

void mock_boot0_set_measured_digest(boot0_slot_t slot, uint32_t digest) {
  if (!slot_is_valid(slot)) {
    state.invalid_access = true;
    return;
  }
  state.measured_digests[slot] = digest;
}

void mock_boot0_set_signature_valid(boot0_slot_t slot, bool valid) {
  if (!slot_is_valid(slot)) {
    state.invalid_access = true;
    return;
  }
  state.signature_valid[slot] = valid;
}

boot0_slot_t mock_boot0_boot_slot(void) {
  return state.boot_slot;
}

uint32_t mock_boot0_recovery_lock(void) {
  return state.recovery_lock;
}

size_t mock_boot0_event_count(void) {
  return state.event_count;
}

mock_boot0_event_t mock_boot0_event_at(size_t index) {
  return index < state.event_count && index < MOCK_BOOT0_HISTORY_CAPACITY
    ? state.events[index].event
    : MOCK_BOOT0_EVENT_COUNT;
}

uint32_t mock_boot0_event_value(size_t index) {
  return index < state.event_count && index < MOCK_BOOT0_HISTORY_CAPACITY
    ? state.events[index].value
    : UINT32_MAX;
}

bool mock_boot0_invalid_access(void) {
  return state.invalid_access;
}

bool boot0_read_header(
  const volatile boot0_registers_t *boot,
  boot0_slot_t slot,
  boot_image_header_t *header
) {
  if (!is_boot(boot) || !slot_is_valid(slot) || header == NULL) {
    state.invalid_access = true;
    return false;
  }
  record_event(MOCK_BOOT0_EVENT_HEADER_READ, (uint32_t)slot);
  if (!state.header_present[slot]) return false;
  *header = state.headers[slot];
  return true;
}

uint32_t boot0_measure_image(
  const volatile boot0_registers_t *boot,
  boot0_slot_t slot
) {
  if (!is_boot(boot) || !slot_is_valid(slot)) {
    state.invalid_access = true;
    return 0u;
  }
  record_event(MOCK_BOOT0_EVENT_IMAGE_MEASURE, (uint32_t)slot);
  return state.measured_digests[slot];
}

bool boot0_verify_signature(
  const volatile boot0_registers_t *boot,
  boot0_slot_t slot,
  uint32_t measured_digest,
  uint32_t signature_tag
) {
  if (!is_boot(boot) || !slot_is_valid(slot)) {
    state.invalid_access = true;
    return false;
  }
  record_event(MOCK_BOOT0_EVENT_SIGNATURE_VERIFY, (uint32_t)slot);
  return state.header_present[slot] &&
    measured_digest == state.measured_digests[slot] &&
    signature_tag == state.headers[slot].signature_tag &&
    state.signature_valid[slot];
}

void boot0_write_boot_slot(
  volatile boot0_registers_t *boot,
  boot0_slot_t slot
) {
  if (!is_boot(boot) || !slot_is_valid(slot)) {
    state.invalid_access = true;
    return;
  }
  state.boot_slot = slot;
  record_event(MOCK_BOOT0_EVENT_BOOT_SLOT_WRITE, (uint32_t)slot);
}

void boot0_write_recovery_lock(
  volatile boot0_registers_t *boot,
  uint32_t value
) {
  if (!is_boot(boot) || (
    value != BOOT0_RECOVERY_LOCKED && value != BOOT0_RECOVERY_UNLOCKED
  )) {
    state.invalid_access = true;
    return;
  }
  state.recovery_lock = value;
  record_event(MOCK_BOOT0_EVENT_RECOVERY_LOCK_WRITE, value);
}

#include "mock_dual_slot_update.h"

#define MOCK_FLASH0_HISTORY_CAPACITY 160u

struct flash0_registers {
  uint32_t reserved;
};

typedef struct {
  mock_flash0_event_t event;
  uint32_t value;
} mock_flash0_event_record_t;

typedef struct {
  struct flash0_registers flash;
  uint32_t words[2][UPDATE_MAX_CHUNKS];
  size_t programmed_counts[2];
  bool erased[2];
  bool verify_valid[2];
  update_slot_t boot_slot;
  uint32_t irq_state;
  mock_flash0_event_record_t events[MOCK_FLASH0_HISTORY_CAPACITY];
  size_t event_count;
  bool invalid_access;
} mock_flash0_state_t;

static mock_flash0_state_t state;

static bool slot_is_valid(update_slot_t slot) {
  return slot == UPDATE_SLOT_A || slot == UPDATE_SLOT_B;
}

static bool is_flash(const volatile flash0_registers_t *flash) {
  return flash == &state.flash;
}

static uint32_t slot_value(update_slot_t slot) {
  return (uint32_t)slot;
}

static uint32_t program_value(update_slot_t slot, uint8_t chunk_index) {
  return ((uint32_t)slot << 16) | (uint32_t)chunk_index;
}

static uint32_t verify_value(update_slot_t slot, uint32_t version) {
  return ((uint32_t)slot << 16) | (version & UINT32_C(0xFFFF));
}

static void record_event(mock_flash0_event_t event, uint32_t value) {
  if (state.event_count < MOCK_FLASH0_HISTORY_CAPACITY) {
    state.events[state.event_count] = (mock_flash0_event_record_t) {
      .event = event,
      .value = value,
    };
  } else {
    state.invalid_access = true;
  }
  state.event_count++;
}

void mock_flash0_reset(void) {
  state = (mock_flash0_state_t) {
    .verify_valid = { true, true },
    .boot_slot = UPDATE_SLOT_NONE,
    .irq_state = UINT32_C(1),
  };
}

volatile flash0_registers_t *mock_flash0(void) {
  return &state.flash;
}

void mock_flash0_set_verify_valid(update_slot_t slot, bool valid) {
  if (!slot_is_valid(slot)) {
    state.invalid_access = true;
    return;
  }
  state.verify_valid[slot] = valid;
}

void mock_flash0_set_irq_state(uint32_t irq_state) {
  state.irq_state = irq_state;
}

update_slot_t mock_flash0_boot_slot(void) {
  return state.boot_slot;
}

size_t mock_flash0_programmed_count(update_slot_t slot) {
  if (!slot_is_valid(slot)) return 0u;
  return state.programmed_counts[slot];
}

uint32_t mock_flash0_irq_state(void) {
  return state.irq_state;
}

size_t mock_flash0_event_count(void) {
  return state.event_count;
}

mock_flash0_event_t mock_flash0_event_at(size_t index) {
  return index < state.event_count && index < MOCK_FLASH0_HISTORY_CAPACITY
    ? state.events[index].event
    : MOCK_FLASH0_EVENT_COUNT;
}

uint32_t mock_flash0_event_value(size_t index) {
  return index < state.event_count && index < MOCK_FLASH0_HISTORY_CAPACITY
    ? state.events[index].value
    : UINT32_MAX;
}

bool mock_flash0_invalid_access(void) {
  return state.invalid_access;
}

void flash0_erase_slot(
  volatile flash0_registers_t *flash,
  update_slot_t slot
) {
  if (!is_flash(flash) || !slot_is_valid(slot)) {
    state.invalid_access = true;
    return;
  }
  state.erased[slot] = true;
  state.programmed_counts[slot] = 0u;
  for (size_t index = 0u; index < UPDATE_MAX_CHUNKS; index++) {
    state.words[slot][index] = UINT32_MAX;
  }
  record_event(MOCK_FLASH0_EVENT_ERASE, slot_value(slot));
}

void flash0_program_word(
  volatile flash0_registers_t *flash,
  update_slot_t slot,
  uint8_t chunk_index,
  uint32_t word
) {
  if (
    !is_flash(flash) || !slot_is_valid(slot) ||
    chunk_index >= UPDATE_MAX_CHUNKS || !state.erased[slot]
  ) {
    state.invalid_access = true;
    return;
  }
  state.words[slot][chunk_index] = word;
  if ((size_t)chunk_index >= state.programmed_counts[slot]) {
    state.programmed_counts[slot] = (size_t)chunk_index + 1u;
  }
  record_event(MOCK_FLASH0_EVENT_PROGRAM, program_value(slot, chunk_index));
}

bool flash0_verify_slot(
  const volatile flash0_registers_t *flash,
  update_slot_t slot,
  uint32_t version
) {
  if (!is_flash(flash) || !slot_is_valid(slot) || version == 0u ||
    version > UPDATE_MAX_VERSION) {
    state.invalid_access = true;
    return false;
  }
  record_event(MOCK_FLASH0_EVENT_VERIFY, verify_value(slot, version));
  return state.verify_valid[slot];
}

void flash0_write_boot_slot(
  volatile flash0_registers_t *flash,
  update_slot_t slot
) {
  if (!is_flash(flash) || !slot_is_valid(slot)) {
    state.invalid_access = true;
    return;
  }
  state.boot_slot = slot;
  record_event(MOCK_FLASH0_EVENT_BOOT_SLOT_WRITE, slot_value(slot));
}

uint32_t flash0_irq_save_disable(void) {
  const uint32_t previous_state = state.irq_state;

  state.irq_state = 0u;
  record_event(MOCK_FLASH0_EVENT_IRQ_SAVE_DISABLE, previous_state);
  return previous_state;
}

void flash0_irq_restore(uint32_t irq_state) {
  state.irq_state = irq_state;
  record_event(MOCK_FLASH0_EVENT_IRQ_RESTORE, irq_state);
}

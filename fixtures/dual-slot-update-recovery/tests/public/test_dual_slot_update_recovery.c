#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "dual_slot_update_recovery.h"
#include "mock_dual_slot_update.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
        __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

typedef struct {
  mock_flash0_event_t event;
  uint32_t value;
} expected_event_t;

static bool slot_is_valid(update_slot_t slot) {
  return slot == UPDATE_SLOT_A || slot == UPDATE_SLOT_B;
}

static uint32_t record_checksum(const update_record_t *record) {
  return record->magic ^ record->confirmed_version ^ record->candidate_version ^
    (uint32_t)record->confirmed_slot ^
    ((uint32_t)record->candidate_slot << 4) ^
    ((uint32_t)record->next_chunk << 8) ^
    ((uint32_t)record->total_chunks << 16) ^
    ((uint32_t)record->phase << 24) ^
    UPDATE_RECORD_CHECKSUM_XOR;
}

static bool record_is_valid(const update_record_t *record) {
  if (
    record->magic != UPDATE_RECORD_MAGIC ||
    !slot_is_valid((update_slot_t)record->confirmed_slot) ||
    record->confirmed_version == 0u ||
    record->confirmed_version > UPDATE_MAX_VERSION ||
    record->reserved[0] != 0u || record->reserved[1] != 0u ||
    record->reserved[2] != 0u || record->checksum != record_checksum(record)
  ) {
    return false;
  }
  if (record->phase == UPDATE_PHASE_IDLE) {
    return record->candidate_slot == UPDATE_SLOT_NONE &&
      record->candidate_version == 0u && record->next_chunk == 0u &&
      record->total_chunks == 0u;
  }
  return slot_is_valid((update_slot_t)record->candidate_slot) &&
    record->candidate_slot != record->confirmed_slot &&
    record->candidate_version > record->confirmed_version &&
    record->candidate_version <= UPDATE_MAX_VERSION &&
    record->total_chunks > 0u &&
    record->total_chunks <= UPDATE_MAX_CHUNKS &&
    record->next_chunk <= record->total_chunks &&
    (record->phase == UPDATE_PHASE_WRITING ||
      ((record->phase == UPDATE_PHASE_TRIAL ||
        record->phase == UPDATE_PHASE_ATTEMPTED) &&
        record->next_chunk == record->total_chunks));
}

static bool record_equals(
  const update_record_t *left,
  const update_record_t *right
) {
  return left->magic == right->magic &&
    left->confirmed_version == right->confirmed_version &&
    left->candidate_version == right->candidate_version &&
    left->confirmed_slot == right->confirmed_slot &&
    left->candidate_slot == right->candidate_slot &&
    left->next_chunk == right->next_chunk &&
    left->total_chunks == right->total_chunks &&
    left->phase == right->phase &&
    left->reserved[0] == right->reserved[0] &&
    left->reserved[1] == right->reserved[1] &&
    left->reserved[2] == right->reserved[2] &&
    left->checksum == right->checksum;
}

static bool manager_equals(
  const update_manager_t *left,
  const update_manager_t *right
) {
  return left->flash == right->flash && left->record == right->record &&
    left->event == right->event && left->initialized == right->initialized;
}

static uint32_t program_value(update_slot_t slot, uint8_t chunk) {
  return ((uint32_t)slot << 16) | (uint32_t)chunk;
}

static uint32_t verify_value(update_slot_t slot, uint32_t version) {
  return ((uint32_t)slot << 16) | (version & UINT32_C(0xFFFF));
}

static bool events_match_from(
  size_t offset,
  const expected_event_t *expected,
  size_t expected_count
) {
  if (mock_flash0_event_count() != offset + expected_count) return false;
  for (size_t index = 0u; index < expected_count; index++) {
    if (
      mock_flash0_event_at(offset + index) != expected[index].event ||
      mock_flash0_event_value(offset + index) != expected[index].value
    ) {
      return false;
    }
  }
  return true;
}

static bool initialize(
  update_manager_t *manager,
  update_record_t *record
) {
  return update_manager_init(
    manager,
    mock_flash0(),
    record,
    UPDATE_SLOT_A,
    UINT32_C(5)
  );
}

static bool stage_one_chunk(
  update_manager_t *manager,
  uint32_t version,
  uint32_t word
) {
  return update_manager_start(manager, UPDATE_SLOT_B, version, UINT8_C(1)) &&
    update_manager_write_chunk(manager, word) &&
    update_manager_finalize(manager);
}

static bool test_initialization_repairs_only_untrusted_records(void) {
  update_manager_t manager = {
    .flash = (volatile flash0_registers_t *)(uintptr_t)UINT32_C(1),
    .record = (update_record_t *)(uintptr_t)UINT32_C(1),
    .event = UPDATE_EVENT_CONFIRMED,
    .initialized = true,
  };
  update_record_t record = {
    .magic = UINT32_C(1),
    .confirmed_version = UINT32_C(2),
    .candidate_version = UINT32_C(3),
    .confirmed_slot = UINT8_C(7),
    .candidate_slot = UINT8_C(8),
    .next_chunk = UINT8_C(9),
    .total_chunks = UINT8_C(10),
    .phase = UINT8_C(11),
    .reserved = { UINT8_C(12), UINT8_C(13), UINT8_C(14) },
    .checksum = UINT32_C(15),
  };
  const update_manager_t before_manager = manager;
  const update_record_t before_record = record;

  mock_flash0_reset();
  CHECK(!update_manager_init(
    NULL, mock_flash0(), &record, UPDATE_SLOT_A, UINT32_C(5)
  ));
  CHECK(!update_manager_init(
    &manager, NULL, &record, UPDATE_SLOT_A, UINT32_C(5)
  ));
  CHECK(!update_manager_init(
    &manager, mock_flash0(), NULL, UPDATE_SLOT_A, UINT32_C(5)
  ));
  CHECK(!update_manager_init(
    &manager, mock_flash0(), &record, UPDATE_SLOT_NONE, UINT32_C(5)
  ));
  CHECK(!update_manager_init(
    &manager, mock_flash0(), &record, UPDATE_SLOT_A, 0u
  ));
  CHECK(!update_manager_init(
    &manager,
    mock_flash0(),
    &record,
    UPDATE_SLOT_A,
    UPDATE_MAX_VERSION + UINT32_C(1)
  ));
  CHECK(manager_equals(&manager, &before_manager));
  CHECK(record_equals(&record, &before_record));
  CHECK(mock_flash0_event_count() == 0u);

  CHECK(initialize(&manager, &record));
  CHECK(record_is_valid(&record));
  CHECK(record.phase == UPDATE_PHASE_IDLE);
  CHECK(record.confirmed_slot == UPDATE_SLOT_A);
  CHECK(record.confirmed_version == UINT32_C(5));
  CHECK(manager.event == UPDATE_EVENT_NONE);
  CHECK(manager.initialized);

  record.confirmed_version = UINT32_C(4);
  record.checksum = record_checksum(&record);
  CHECK(record_is_valid(&record));
  CHECK(initialize(&manager, &record));
  CHECK(record_is_valid(&record));
  CHECK(record.phase == UPDATE_PHASE_IDLE);
  CHECK(record.confirmed_slot == UPDATE_SLOT_A);
  CHECK(record.confirmed_version == UINT32_C(5));

  CHECK(mock_flash0_event_count() == 0u);
  CHECK(!mock_flash0_invalid_access());
  return true;
}

static bool test_start_requires_a_strict_newer_version_and_orders_chunks(void) {
  update_manager_t manager = { 0 };
  update_record_t record = { 0 };
  const expected_event_t start_events[] = {
    { MOCK_FLASH0_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xA5) },
    { MOCK_FLASH0_EVENT_ERASE, UPDATE_SLOT_B },
    { MOCK_FLASH0_EVENT_IRQ_RESTORE, UINT32_C(0xA5) },
  };
  const expected_event_t first_chunk_events[] = {
    { MOCK_FLASH0_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xA5) },
    { MOCK_FLASH0_EVENT_PROGRAM, program_value(UPDATE_SLOT_B, UINT8_C(0)) },
    { MOCK_FLASH0_EVENT_IRQ_RESTORE, UINT32_C(0xA5) },
  };
  const expected_event_t finalize_events[] = {
    { MOCK_FLASH0_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xA5) },
    { MOCK_FLASH0_EVENT_VERIFY, verify_value(UPDATE_SLOT_B, UINT32_C(6)) },
    { MOCK_FLASH0_EVENT_IRQ_RESTORE, UINT32_C(0xA5) },
  };
  size_t offset;

  mock_flash0_reset();
  CHECK(initialize(&manager, &record));
  CHECK(!update_manager_start(
    &manager,
    UPDATE_SLOT_A,
    UINT32_C(6),
    UINT8_C(2)
  ));
  CHECK(!update_manager_start(
    &manager,
    UPDATE_SLOT_B,
    UINT32_C(5),
    UINT8_C(2)
  ));
  CHECK(!update_manager_start(
    &manager,
    UPDATE_SLOT_B,
    UINT32_C(6),
    0u
  ));
  CHECK(mock_flash0_event_count() == 0u);

  mock_flash0_set_irq_state(UINT32_C(0xA5));
  CHECK(update_manager_start(
    &manager,
    UPDATE_SLOT_B,
    UINT32_C(6),
    UINT8_C(2)
  ));
  CHECK(events_match_from(
    0u,
    start_events,
    sizeof(start_events) / sizeof(start_events[0])
  ));
  CHECK(record.phase == UPDATE_PHASE_WRITING);
  CHECK(record.candidate_slot == UPDATE_SLOT_B);
  CHECK(record.candidate_version == UINT32_C(6));
  CHECK(record.next_chunk == 0u);
  CHECK(record.total_chunks == UINT8_C(2));
  CHECK(record_is_valid(&record));

  offset = mock_flash0_event_count();
  CHECK(update_manager_write_chunk(&manager, UINT32_C(0x11111111)));
  CHECK(events_match_from(
    offset,
    first_chunk_events,
    sizeof(first_chunk_events) / sizeof(first_chunk_events[0])
  ));
  CHECK(record.next_chunk == UINT8_C(1));
  CHECK(update_manager_write_chunk(&manager, UINT32_C(0x22222222)));
  CHECK(record.next_chunk == UINT8_C(2));
  CHECK(mock_flash0_programmed_count(UPDATE_SLOT_B) == 2u);

  offset = mock_flash0_event_count();
  CHECK(update_manager_finalize(&manager));
  CHECK(events_match_from(
    offset,
    finalize_events,
    sizeof(finalize_events) / sizeof(finalize_events[0])
  ));
  CHECK(record.phase == UPDATE_PHASE_TRIAL);
  CHECK(record_is_valid(&record));
  CHECK(mock_flash0_irq_state() == UINT32_C(0xA5));
  CHECK(!mock_flash0_invalid_access());
  return true;
}

static bool test_interruption_and_failed_verification_preserve_confirmed_boot(void) {
  update_manager_t manager = { 0 };
  update_manager_t rebooted = { 0 };
  update_record_t record = { 0 };
  const expected_event_t interrupted_boot_events[] = {
    { MOCK_FLASH0_EVENT_ERASE, UPDATE_SLOT_B },
    { MOCK_FLASH0_EVENT_BOOT_SLOT_WRITE, UPDATE_SLOT_A },
  };
  const expected_event_t rejected_events[] = {
    { MOCK_FLASH0_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xB4) },
    { MOCK_FLASH0_EVENT_VERIFY, verify_value(UPDATE_SLOT_B, UINT32_C(6)) },
    { MOCK_FLASH0_EVENT_ERASE, UPDATE_SLOT_B },
    { MOCK_FLASH0_EVENT_IRQ_RESTORE, UINT32_C(0xB4) },
  };
  size_t offset;

  mock_flash0_reset();
  CHECK(initialize(&manager, &record));
  CHECK(update_manager_start(
    &manager,
    UPDATE_SLOT_B,
    UINT32_C(6),
    UINT8_C(2)
  ));
  CHECK(update_manager_write_chunk(&manager, UINT32_C(0x11223344)));
  CHECK(initialize(&rebooted, &record));
  offset = mock_flash0_event_count();
  CHECK(update_manager_boot(&rebooted) == UPDATE_BOOT_ROLLED_BACK);
  CHECK(events_match_from(
    offset,
    interrupted_boot_events,
    sizeof(interrupted_boot_events) / sizeof(interrupted_boot_events[0])
  ));
  CHECK(record.phase == UPDATE_PHASE_IDLE);
  CHECK(record.confirmed_slot == UPDATE_SLOT_A);
  CHECK(mock_flash0_boot_slot() == UPDATE_SLOT_A);
  mock_flash0_set_irq_state(UINT32_C(0xB4));
  CHECK(update_manager_take_event(&rebooted) == UPDATE_EVENT_ROLLED_BACK);
  CHECK(mock_flash0_irq_state() == UINT32_C(0xB4));

  mock_flash0_reset();
  record = (update_record_t) { 0 };
  manager = (update_manager_t) { 0 };
  CHECK(initialize(&manager, &record));
  CHECK(update_manager_start(
    &manager,
    UPDATE_SLOT_B,
    UINT32_C(6),
    UINT8_C(1)
  ));
  CHECK(update_manager_write_chunk(&manager, UINT32_C(0x55667788)));
  mock_flash0_set_verify_valid(UPDATE_SLOT_B, false);
  mock_flash0_set_irq_state(UINT32_C(0xB4));
  offset = mock_flash0_event_count();
  CHECK(!update_manager_finalize(&manager));
  CHECK(events_match_from(
    offset,
    rejected_events,
    sizeof(rejected_events) / sizeof(rejected_events[0])
  ));
  CHECK(record.phase == UPDATE_PHASE_IDLE);
  CHECK(update_manager_take_event(&manager) == UPDATE_EVENT_CANDIDATE_REJECTED);
  CHECK(mock_flash0_boot_slot() == UPDATE_SLOT_NONE);
  CHECK(!mock_flash0_invalid_access());
  return true;
}

static bool test_trial_confirmation_and_unconfirmed_rollback(void) {
  update_manager_t manager = { 0 };
  update_manager_t rebooted = { 0 };
  update_record_t record = { 0 };
  const expected_event_t trial_boot_events[] = {
    { MOCK_FLASH0_EVENT_VERIFY, verify_value(UPDATE_SLOT_B, UINT32_C(6)) },
    { MOCK_FLASH0_EVENT_BOOT_SLOT_WRITE, UPDATE_SLOT_B },
  };
  const expected_event_t rollback_events[] = {
    { MOCK_FLASH0_EVENT_ERASE, UPDATE_SLOT_B },
    { MOCK_FLASH0_EVENT_BOOT_SLOT_WRITE, UPDATE_SLOT_A },
  };
  size_t offset;

  mock_flash0_reset();
  CHECK(initialize(&manager, &record));
  CHECK(stage_one_chunk(&manager, UINT32_C(6), UINT32_C(0x12345678)));
  offset = mock_flash0_event_count();
  CHECK(update_manager_boot(&manager) == UPDATE_BOOT_TRIAL);
  CHECK(events_match_from(
    offset,
    trial_boot_events,
    sizeof(trial_boot_events) / sizeof(trial_boot_events[0])
  ));
  CHECK(record.phase == UPDATE_PHASE_ATTEMPTED);
  mock_flash0_set_irq_state(UINT32_C(0xA5));
  CHECK(update_manager_confirm(&manager));
  CHECK(record.phase == UPDATE_PHASE_IDLE);
  CHECK(record.confirmed_slot == UPDATE_SLOT_B);
  CHECK(record.confirmed_version == UINT32_C(6));
  CHECK(update_manager_take_event(&manager) == UPDATE_EVENT_CONFIRMED);

  CHECK(initialize(&rebooted, &record));
  offset = mock_flash0_event_count();
  CHECK(update_manager_boot(&rebooted) == UPDATE_BOOT_CONFIRMED);
  CHECK(events_match_from(
    offset,
    (const expected_event_t[]) {
      { MOCK_FLASH0_EVENT_BOOT_SLOT_WRITE, UPDATE_SLOT_B },
    },
    1u
  ));

  mock_flash0_reset();
  record = (update_record_t) { 0 };
  manager = (update_manager_t) { 0 };
  rebooted = (update_manager_t) { 0 };
  CHECK(initialize(&manager, &record));
  CHECK(stage_one_chunk(&manager, UINT32_C(6), UINT32_C(0xAABBCCDD)));
  CHECK(update_manager_boot(&manager) == UPDATE_BOOT_TRIAL);
  CHECK(initialize(&rebooted, &record));
  offset = mock_flash0_event_count();
  CHECK(update_manager_boot(&rebooted) == UPDATE_BOOT_ROLLED_BACK);
  CHECK(events_match_from(
    offset,
    rollback_events,
    sizeof(rollback_events) / sizeof(rollback_events[0])
  ));
  CHECK(record.phase == UPDATE_PHASE_IDLE);
  CHECK(record.confirmed_slot == UPDATE_SLOT_A);
  CHECK(record.confirmed_version == UINT32_C(5));
  CHECK(update_manager_take_event(&rebooted) == UPDATE_EVENT_ROLLED_BACK);
  CHECK(!mock_flash0_invalid_access());
  return true;
}

static bool test_invalid_foreground_transitions_restore_interrupts(void) {
  update_manager_t manager = { 0 };
  update_record_t record = { 0 };
  const expected_event_t expected[] = {
    { MOCK_FLASH0_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0x5A) },
    { MOCK_FLASH0_EVENT_IRQ_RESTORE, UINT32_C(0x5A) },
    { MOCK_FLASH0_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0x5A) },
    { MOCK_FLASH0_EVENT_IRQ_RESTORE, UINT32_C(0x5A) },
    { MOCK_FLASH0_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0x5A) },
    { MOCK_FLASH0_EVENT_IRQ_RESTORE, UINT32_C(0x5A) },
    { MOCK_FLASH0_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0x5A) },
    { MOCK_FLASH0_EVENT_IRQ_RESTORE, UINT32_C(0x5A) },
  };

  mock_flash0_reset();
  CHECK(!update_manager_write_chunk(NULL, UINT32_C(1)));
  CHECK(!update_manager_finalize(NULL));
  CHECK(update_manager_boot(NULL) == UPDATE_BOOT_INVALID);
  CHECK(!update_manager_confirm(NULL));
  CHECK(update_manager_take_event(NULL) == UPDATE_EVENT_NONE);
  CHECK(mock_flash0_event_count() == 0u);

  CHECK(initialize(&manager, &record));
  mock_flash0_set_irq_state(UINT32_C(0x5A));
  CHECK(!update_manager_write_chunk(&manager, UINT32_C(1)));
  CHECK(!update_manager_finalize(&manager));
  CHECK(!update_manager_confirm(&manager));
  CHECK(update_manager_take_event(&manager) == UPDATE_EVENT_NONE);
  CHECK(events_match_from(
    0u,
    expected,
    sizeof(expected) / sizeof(expected[0])
  ));
  CHECK(mock_flash0_irq_state() == UINT32_C(0x5A));
  CHECK(!mock_flash0_invalid_access());
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "initialization repairs untrusted records", test_initialization_repairs_only_untrusted_records },
    { "strict version and chunk ordering", test_start_requires_a_strict_newer_version_and_orders_chunks },
    { "interruption and verification rejection", test_interruption_and_failed_verification_preserve_confirmed_boot },
    { "trial confirmation and rollback", test_trial_confirmation_and_unconfirmed_rollback },
    { "invalid foreground transitions", test_invalid_foreground_transitions_restore_interrupts },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) {
      fprintf(stderr, "failed: %s\n", tests[index].name);
      return 1;
    }
  }

  printf("Dual-slot update recovery public tests passed (%zu tests).\n",
    sizeof(tests) / sizeof(tests[0]));
  return 0;
}

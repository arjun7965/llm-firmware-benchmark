#include <stddef.h>

#include "dual_slot_update_recovery.h"

static bool slot_is_valid(update_slot_t slot) {
  return slot == UPDATE_SLOT_A || slot == UPDATE_SLOT_B;
}

static bool version_is_valid(uint32_t version) {
  return version != 0u && version <= UPDATE_MAX_VERSION;
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
    !version_is_valid(record->confirmed_version) ||
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
  if (
    record->phase != UPDATE_PHASE_WRITING &&
    record->phase != UPDATE_PHASE_TRIAL &&
    record->phase != UPDATE_PHASE_ATTEMPTED
  ) {
    return false;
  }
  if (
    !slot_is_valid((update_slot_t)record->candidate_slot) ||
    record->candidate_slot == record->confirmed_slot ||
    !version_is_valid(record->candidate_version) ||
    record->candidate_version <= record->confirmed_version ||
    record->total_chunks == 0u || record->total_chunks > UPDATE_MAX_CHUNKS ||
    record->next_chunk > record->total_chunks
  ) {
    return false;
  }
  return record->phase == UPDATE_PHASE_WRITING ||
    record->next_chunk == record->total_chunks;
}

static void write_record(
  update_record_t *record,
  update_slot_t confirmed_slot,
  uint32_t confirmed_version,
  update_slot_t candidate_slot,
  uint32_t candidate_version,
  uint8_t next_chunk,
  uint8_t total_chunks,
  update_phase_t phase
) {
  *record = (update_record_t) {
    .magic = UPDATE_RECORD_MAGIC,
    .confirmed_version = confirmed_version,
    .candidate_version = candidate_version,
    .confirmed_slot = (uint8_t)confirmed_slot,
    .candidate_slot = (uint8_t)candidate_slot,
    .next_chunk = next_chunk,
    .total_chunks = total_chunks,
    .phase = (uint8_t)phase,
    .reserved = { 0u, 0u, 0u },
    .checksum = 0u,
  };
  record->checksum = record_checksum(record);
}

static void write_idle(
  update_record_t *record,
  update_slot_t confirmed_slot,
  uint32_t confirmed_version
) {
  write_record(
    record,
    confirmed_slot,
    confirmed_version,
    UPDATE_SLOT_NONE,
    0u,
    0u,
    0u,
    UPDATE_PHASE_IDLE
  );
}

static bool manager_is_ready(const update_manager_t *manager) {
  return manager != NULL && manager->initialized && manager->flash != NULL &&
    manager->record != NULL;
}

bool update_manager_init(
  update_manager_t *manager,
  volatile flash0_registers_t *flash,
  update_record_t *record,
  update_slot_t factory_slot,
  uint32_t factory_version
) {
  if (
    manager == NULL || flash == NULL || record == NULL ||
    !slot_is_valid(factory_slot) || !version_is_valid(factory_version)
  ) {
    return false;
  }
  if (
    !record_is_valid(record) ||
    record->confirmed_version < factory_version
  ) {
    write_idle(record, factory_slot, factory_version);
  }
  *manager = (update_manager_t) {
    .flash = flash,
    .record = record,
    .event = UPDATE_EVENT_NONE,
    .initialized = true,
  };
  return true;
}

bool update_manager_start(
  update_manager_t *manager,
  update_slot_t target_slot,
  uint32_t candidate_version,
  uint8_t total_chunks
) {
  uint32_t irq_state;

  if (
    !manager_is_ready(manager) || !slot_is_valid(target_slot) ||
    target_slot == (update_slot_t)manager->record->confirmed_slot ||
    !version_is_valid(candidate_version) ||
    candidate_version <= manager->record->confirmed_version ||
    total_chunks == 0u || total_chunks > UPDATE_MAX_CHUNKS
  ) {
    return false;
  }

  irq_state = flash0_irq_save_disable();
  if (
    manager->record->phase != UPDATE_PHASE_IDLE ||
    manager->event != UPDATE_EVENT_NONE
  ) {
    flash0_irq_restore(irq_state);
    return false;
  }

  flash0_erase_slot(manager->flash, target_slot);
  write_record(
    manager->record,
    (update_slot_t)manager->record->confirmed_slot,
    manager->record->confirmed_version,
    target_slot,
    candidate_version,
    0u,
    total_chunks,
    UPDATE_PHASE_WRITING
  );
  flash0_irq_restore(irq_state);
  return true;
}

bool update_manager_write_chunk(update_manager_t *manager, uint32_t word) {
  uint32_t irq_state;

  if (!manager_is_ready(manager)) return false;

  irq_state = flash0_irq_save_disable();
  if (
    manager->record->phase != UPDATE_PHASE_WRITING ||
    manager->record->next_chunk >= manager->record->total_chunks ||
    manager->event != UPDATE_EVENT_NONE
  ) {
    flash0_irq_restore(irq_state);
    return false;
  }

  flash0_program_word(
    manager->flash,
    (update_slot_t)manager->record->candidate_slot,
    manager->record->next_chunk,
    word
  );
  write_record(
    manager->record,
    (update_slot_t)manager->record->confirmed_slot,
    manager->record->confirmed_version,
    (update_slot_t)manager->record->candidate_slot,
    manager->record->candidate_version,
    (uint8_t)(manager->record->next_chunk + UINT8_C(1)),
    manager->record->total_chunks,
    UPDATE_PHASE_WRITING
  );
  flash0_irq_restore(irq_state);
  return true;
}

bool update_manager_finalize(update_manager_t *manager) {
  uint32_t irq_state;

  if (!manager_is_ready(manager)) return false;

  irq_state = flash0_irq_save_disable();
  if (
    manager->record->phase != UPDATE_PHASE_WRITING ||
    manager->record->next_chunk != manager->record->total_chunks ||
    manager->event != UPDATE_EVENT_NONE
  ) {
    flash0_irq_restore(irq_state);
    return false;
  }
  if (!flash0_verify_slot(
    manager->flash,
    (update_slot_t)manager->record->candidate_slot,
    manager->record->candidate_version
  )) {
    const update_slot_t candidate_slot =
      (update_slot_t)manager->record->candidate_slot;

    write_idle(
      manager->record,
      (update_slot_t)manager->record->confirmed_slot,
      manager->record->confirmed_version
    );
    flash0_erase_slot(manager->flash, candidate_slot);
    manager->event = UPDATE_EVENT_CANDIDATE_REJECTED;
    flash0_irq_restore(irq_state);
    return false;
  }

  write_record(
    manager->record,
    (update_slot_t)manager->record->confirmed_slot,
    manager->record->confirmed_version,
    (update_slot_t)manager->record->candidate_slot,
    manager->record->candidate_version,
    manager->record->total_chunks,
    manager->record->total_chunks,
    UPDATE_PHASE_TRIAL
  );
  flash0_irq_restore(irq_state);
  return true;
}

update_boot_result_t update_manager_boot(update_manager_t *manager) {
  update_slot_t candidate_slot;
  update_slot_t confirmed_slot;
  uint32_t confirmed_version;

  if (!manager_is_ready(manager)) return UPDATE_BOOT_INVALID;

  confirmed_slot = (update_slot_t)manager->record->confirmed_slot;
  confirmed_version = manager->record->confirmed_version;
  candidate_slot = (update_slot_t)manager->record->candidate_slot;
  switch ((update_phase_t)manager->record->phase) {
    case UPDATE_PHASE_IDLE:
      flash0_write_boot_slot(manager->flash, confirmed_slot);
      return UPDATE_BOOT_CONFIRMED;
    case UPDATE_PHASE_WRITING:
      write_idle(manager->record, confirmed_slot, confirmed_version);
      flash0_erase_slot(manager->flash, candidate_slot);
      flash0_write_boot_slot(manager->flash, confirmed_slot);
      manager->event = UPDATE_EVENT_ROLLED_BACK;
      return UPDATE_BOOT_ROLLED_BACK;
    case UPDATE_PHASE_TRIAL:
      if (!flash0_verify_slot(
        manager->flash,
        candidate_slot,
        manager->record->candidate_version
      )) {
        write_idle(manager->record, confirmed_slot, confirmed_version);
        flash0_erase_slot(manager->flash, candidate_slot);
        flash0_write_boot_slot(manager->flash, confirmed_slot);
        manager->event = UPDATE_EVENT_ROLLED_BACK;
        return UPDATE_BOOT_ROLLED_BACK;
      }
      write_record(
        manager->record,
        confirmed_slot,
        confirmed_version,
        candidate_slot,
        manager->record->candidate_version,
        manager->record->total_chunks,
        manager->record->total_chunks,
        UPDATE_PHASE_ATTEMPTED
      );
      flash0_write_boot_slot(manager->flash, candidate_slot);
      return UPDATE_BOOT_TRIAL;
    case UPDATE_PHASE_ATTEMPTED:
      write_idle(manager->record, confirmed_slot, confirmed_version);
      flash0_erase_slot(manager->flash, candidate_slot);
      flash0_write_boot_slot(manager->flash, confirmed_slot);
      manager->event = UPDATE_EVENT_ROLLED_BACK;
      return UPDATE_BOOT_ROLLED_BACK;
    default:
      return UPDATE_BOOT_INVALID;
  }
}

bool update_manager_confirm(update_manager_t *manager) {
  uint32_t irq_state;

  if (!manager_is_ready(manager)) return false;

  irq_state = flash0_irq_save_disable();
  if (
    manager->record->phase != UPDATE_PHASE_ATTEMPTED ||
    manager->event != UPDATE_EVENT_NONE
  ) {
    flash0_irq_restore(irq_state);
    return false;
  }
  write_idle(
    manager->record,
    (update_slot_t)manager->record->candidate_slot,
    manager->record->candidate_version
  );
  manager->event = UPDATE_EVENT_CONFIRMED;
  flash0_irq_restore(irq_state);
  return true;
}

update_event_t update_manager_take_event(update_manager_t *manager) {
  update_event_t event;
  uint32_t irq_state;

  if (!manager_is_ready(manager)) return UPDATE_EVENT_NONE;

  irq_state = flash0_irq_save_disable();
  event = manager->event;
  manager->event = UPDATE_EVENT_NONE;
  flash0_irq_restore(irq_state);
  return event;
}

#ifndef DUAL_SLOT_UPDATE_RECOVERY_H
#define DUAL_SLOT_UPDATE_RECOVERY_H

#include <stdbool.h>
#include <stdint.h>

#include "fixture_dual_slot_update.h"

#define UPDATE_RECORD_MAGIC UINT32_C(0x55504454)
#define UPDATE_RECORD_CHECKSUM_XOR UINT32_C(0x7CB39E15)

typedef enum {
  UPDATE_PHASE_IDLE = 0,
  UPDATE_PHASE_WRITING,
  UPDATE_PHASE_TRIAL,
  UPDATE_PHASE_ATTEMPTED,
} update_phase_t;

typedef enum {
  UPDATE_BOOT_INVALID = 0,
  UPDATE_BOOT_CONFIRMED,
  UPDATE_BOOT_TRIAL,
  UPDATE_BOOT_ROLLED_BACK,
} update_boot_result_t;

typedef enum {
  UPDATE_EVENT_NONE = 0,
  UPDATE_EVENT_CANDIDATE_REJECTED,
  UPDATE_EVENT_CONFIRMED,
  UPDATE_EVENT_ROLLED_BACK,
} update_event_t;

typedef struct {
  uint32_t magic;
  uint32_t confirmed_version;
  uint32_t candidate_version;
  uint8_t confirmed_slot;
  uint8_t candidate_slot;
  uint8_t next_chunk;
  uint8_t total_chunks;
  uint8_t phase;
  uint8_t reserved[3];
  uint32_t checksum;
} update_record_t;

typedef struct {
  volatile flash0_registers_t *flash;
  update_record_t *record;
  update_event_t event;
  bool initialized;
} update_manager_t;

bool update_manager_init(
  update_manager_t *manager,
  volatile flash0_registers_t *flash,
  update_record_t *record,
  update_slot_t factory_slot,
  uint32_t factory_version
);
bool update_manager_start(
  update_manager_t *manager,
  update_slot_t target_slot,
  uint32_t candidate_version,
  uint8_t total_chunks
);
bool update_manager_write_chunk(update_manager_t *manager, uint32_t word);
bool update_manager_finalize(update_manager_t *manager);
update_boot_result_t update_manager_boot(update_manager_t *manager);
bool update_manager_confirm(update_manager_t *manager);
update_event_t update_manager_take_event(update_manager_t *manager);

#endif

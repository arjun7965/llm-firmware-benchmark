#ifndef FIXTURE_DUAL_SLOT_UPDATE_H
#define FIXTURE_DUAL_SLOT_UPDATE_H

#include <stdbool.h>
#include <stdint.h>

#define UPDATE_MAX_CHUNKS UINT8_C(4)
#define UPDATE_MAX_VERSION UINT32_C(4095)

typedef enum {
  UPDATE_SLOT_A = 0,
  UPDATE_SLOT_B = 1,
  UPDATE_SLOT_NONE = 2,
} update_slot_t;

typedef struct flash0_registers flash0_registers_t;

void flash0_erase_slot(
  volatile flash0_registers_t *flash,
  update_slot_t slot
);
void flash0_program_word(
  volatile flash0_registers_t *flash,
  update_slot_t slot,
  uint8_t chunk_index,
  uint32_t word
);
bool flash0_verify_slot(
  const volatile flash0_registers_t *flash,
  update_slot_t slot,
  uint32_t version
);
void flash0_write_boot_slot(
  volatile flash0_registers_t *flash,
  update_slot_t slot
);
uint32_t flash0_irq_save_disable(void);
void flash0_irq_restore(uint32_t state);

#endif

#ifndef FIXTURE_VECTOR_TABLE_H
#define FIXTURE_VECTOR_TABLE_H

#include <stddef.h>
#include <stdint.h>

#define SCB_BASE_ADDRESS UINT32_C(0xE000ED00)
#define NVIC_BASE_ADDRESS UINT32_C(0xE000E100)
#define SCB_VTOR_OFFSET UINT32_C(0x08)

#define INTERRUPT_VECTOR_ALIGNMENT UINT32_C(128)
#define INTERRUPT_VECTOR_STACK_ALIGNMENT UINT32_C(8)
#define INTERRUPT_VECTOR_CORE_ENTRY_COUNT UINT32_C(16)
#define INTERRUPT_VECTOR_EXTERNAL_IRQ_COUNT UINT32_C(8)
#define INTERRUPT_VECTOR_ENTRY_COUNT \
  (INTERRUPT_VECTOR_CORE_ENTRY_COUNT + INTERRUPT_VECTOR_EXTERNAL_IRQ_COUNT)
#define INTERRUPT_VECTOR_EXTERNAL_MASK UINT32_C(0x000000FF)

#define INTERRUPT_VECTOR_INITIAL_STACK_INDEX UINT32_C(0)
#define INTERRUPT_VECTOR_RESET_INDEX UINT32_C(1)
#define INTERRUPT_VECTOR_FIRST_DEFAULT_INDEX UINT32_C(2)
#define INTERRUPT_VECTOR_HANDLER_THUMB_BIT UINT32_C(1)

typedef struct system_control_block system_control_block_t;
typedef struct nvic nvic_t;

typedef struct {
  _Alignas(INTERRUPT_VECTOR_ALIGNMENT)
  uint32_t entries[INTERRUPT_VECTOR_ENTRY_COUNT];
} interrupt_vector_table_t;

_Static_assert(
  offsetof(interrupt_vector_table_t, entries) == 0u,
  "vector entries must begin at the linker-reserved table base"
);
_Static_assert(
  _Alignof(interrupt_vector_table_t) >= INTERRUPT_VECTOR_ALIGNMENT,
  "vector table must satisfy the VTOR alignment"
);

uintptr_t interrupt_vector_table_address(
  const interrupt_vector_table_t *table
);
void interrupt_vector_write_entry(
  interrupt_vector_table_t *table,
  size_t index,
  uint32_t value
);

void scb_write_vtor(
  volatile system_control_block_t *scb,
  uintptr_t value
);
void nvic_write_icer(volatile nvic_t *nvic, uint32_t value);
void nvic_write_icpr(volatile nvic_t *nvic, uint32_t value);
void nvic_write_iser(volatile nvic_t *nvic, uint32_t value);
void interrupt_vector_sync_barrier(void);

uint32_t interrupt_vector_irq_save_disable(void);
void interrupt_vector_irq_restore(uint32_t state);

#endif

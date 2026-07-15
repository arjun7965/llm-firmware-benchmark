#include "interrupt_vector.h"

#include <stddef.h>

static bool stack_pointer_is_valid(uint32_t initial_stack_pointer) {
  return initial_stack_pointer != 0u &&
    (initial_stack_pointer & (INTERRUPT_VECTOR_STACK_ALIGNMENT - 1u)) == 0u;
}

static bool handler_is_valid(uint32_t handler) {
  return handler != 0u &&
    (handler & INTERRUPT_VECTOR_HANDLER_THUMB_BIT) != 0u;
}

static bool state_is_ready(const interrupt_vector_state_t *state) {
  return state != NULL && state->initialized && state->scb != NULL &&
    state->nvic != NULL && state->table != NULL;
}

bool interrupt_vector_initialize(
  interrupt_vector_state_t *state,
  volatile system_control_block_t *scb,
  volatile nvic_t *nvic,
  interrupt_vector_table_t *table,
  uint32_t initial_stack_pointer,
  uint32_t reset_handler,
  uint32_t default_handler
) {
  if (
    state == NULL || scb == NULL || nvic == NULL || table == NULL ||
    !stack_pointer_is_valid(initial_stack_pointer) ||
    !handler_is_valid(reset_handler) || !handler_is_valid(default_handler)
  ) {
    return false;
  }

  const uintptr_t base_address = interrupt_vector_table_address(table);
  if (
    base_address == 0u ||
    base_address % INTERRUPT_VECTOR_ALIGNMENT != 0u
  ) {
    return false;
  }

  nvic_write_icer(nvic, INTERRUPT_VECTOR_EXTERNAL_MASK);
  nvic_write_icpr(nvic, INTERRUPT_VECTOR_EXTERNAL_MASK);
  interrupt_vector_write_entry(
    table,
    INTERRUPT_VECTOR_INITIAL_STACK_INDEX,
    initial_stack_pointer
  );
  interrupt_vector_write_entry(
    table,
    INTERRUPT_VECTOR_RESET_INDEX,
    reset_handler
  );
  for (
    size_t index = INTERRUPT_VECTOR_FIRST_DEFAULT_INDEX;
    index < INTERRUPT_VECTOR_ENTRY_COUNT;
    index++
  ) {
    interrupt_vector_write_entry(table, index, default_handler);
  }
  interrupt_vector_sync_barrier();
  scb_write_vtor(scb, base_address);
  interrupt_vector_sync_barrier();

  *state = (interrupt_vector_state_t) {
    .scb = scb,
    .nvic = nvic,
    .table = table,
    .initialized = true,
  };
  return true;
}

bool interrupt_vector_install_irq(
  interrupt_vector_state_t *state,
  uint32_t irq_number,
  uint32_t handler
) {
  if (
    !state_is_ready(state) ||
    irq_number >= INTERRUPT_VECTOR_EXTERNAL_IRQ_COUNT ||
    !handler_is_valid(handler)
  ) {
    return false;
  }

  const uint32_t interrupt_state = interrupt_vector_irq_save_disable();
  interrupt_vector_write_entry(
    state->table,
    INTERRUPT_VECTOR_CORE_ENTRY_COUNT + irq_number,
    handler
  );
  interrupt_vector_sync_barrier();
  interrupt_vector_irq_restore(interrupt_state);
  return true;
}

bool interrupt_vector_enable_irq(
  interrupt_vector_state_t *state,
  uint32_t irq_number
) {
  if (
    !state_is_ready(state) ||
    irq_number >= INTERRUPT_VECTOR_EXTERNAL_IRQ_COUNT
  ) {
    return false;
  }

  const uint32_t irq_bit = UINT32_C(1) << irq_number;
  const uint32_t interrupt_state = interrupt_vector_irq_save_disable();
  nvic_write_icpr(state->nvic, irq_bit);
  nvic_write_iser(state->nvic, irq_bit);
  interrupt_vector_irq_restore(interrupt_state);
  return true;
}

#ifndef INTERRUPT_VECTOR_H
#define INTERRUPT_VECTOR_H

#include <stdbool.h>
#include <stdint.h>

#include "fixture_vector_table.h"

typedef struct {
  volatile system_control_block_t *scb;
  volatile nvic_t *nvic;
  interrupt_vector_table_t *table;
  bool initialized;
} interrupt_vector_state_t;

bool interrupt_vector_initialize(
  interrupt_vector_state_t *state,
  volatile system_control_block_t *scb,
  volatile nvic_t *nvic,
  interrupt_vector_table_t *table,
  uint32_t initial_stack_pointer,
  uint32_t reset_handler,
  uint32_t default_handler
);
bool interrupt_vector_install_irq(
  interrupt_vector_state_t *state,
  uint32_t irq_number,
  uint32_t handler
);
bool interrupt_vector_enable_irq(
  interrupt_vector_state_t *state,
  uint32_t irq_number
);

#endif

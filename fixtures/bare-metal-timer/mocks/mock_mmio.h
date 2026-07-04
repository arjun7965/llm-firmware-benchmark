#ifndef MOCK_MMIO_H
#define MOCK_MMIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fictional_timer.h"

void mock_timer0_reset(
  volatile timer0_registers_t *timer,
  uint32_t initial_value
);
void mock_timer0_clear_log(void);
void mock_timer0_set_status(
  volatile timer0_registers_t *timer,
  uint32_t status
);
size_t mock_timer0_write_count(void);
uint32_t mock_timer0_write_offset(size_t index);
uint32_t mock_timer0_write_value(size_t index);
bool mock_timer0_invalid_access(void);

#endif

#ifndef MOCK_HAL_H
#define MOCK_HAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void mock_hal_reset(uint32_t now_ms);
bool mock_hal_plan_transfer(
  bool accept,
  uint32_t completion_delay_ms,
  bool success,
  uint8_t first_byte,
  uint8_t second_byte
);
void mock_hal_advance(uint32_t delta_ms);
size_t mock_hal_start_count(void);
uint32_t mock_hal_start_time(size_t index);
bool mock_hal_overlap_detected(void);
bool mock_hal_in_flight(void);
uint8_t mock_hal_last_address(void);
uint8_t mock_hal_last_register(void);
size_t mock_hal_last_length(void);

#endif

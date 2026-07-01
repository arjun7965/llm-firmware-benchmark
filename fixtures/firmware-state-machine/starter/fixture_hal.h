#ifndef FIXTURE_HAL_H
#define FIXTURE_HAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool i2c_start_read(
  uint8_t address,
  uint8_t reg,
  uint8_t *destination,
  size_t length
);
bool i2c_done(void);
bool i2c_ok(void);
uint32_t millis(void);

#endif

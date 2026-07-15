#ifndef I2C_CONTROLLER_H
#define I2C_CONTROLLER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_i2c_controller.h"

#define I2C_CONTROLLER_TIMEOUT_MS UINT32_C(25)
#define I2C_CONTROLLER_MAX_WRITE_BYTES UINT8_C(4)
#define I2C_CONTROLLER_MIN_ADDRESS UINT8_C(0x08)
#define I2C_CONTROLLER_MAX_ADDRESS UINT8_C(0x77)

typedef enum {
  I2C_CONTROLLER_PHASE_IDLE = 0,
  I2C_CONTROLLER_PHASE_WAIT_START,
  I2C_CONTROLLER_PHASE_WAIT_ADDRESS,
  I2C_CONTROLLER_PHASE_WAIT_DATA,
} i2c_controller_phase_t;

typedef enum {
  I2C_CONTROLLER_RESULT_NONE = 0,
  I2C_CONTROLLER_RESULT_COMPLETE,
  I2C_CONTROLLER_RESULT_NACK,
  I2C_CONTROLLER_RESULT_ARBITRATION_LOST,
  I2C_CONTROLLER_RESULT_BUS_ERROR,
  I2C_CONTROLLER_RESULT_TIMED_OUT,
} i2c_controller_result_t;

typedef struct {
  volatile i2c0_registers_t *i2c;
  const uint8_t *data;
  size_t length;
  size_t next_index;
  uint8_t address;
  uint32_t started_at_ms;
  i2c_controller_phase_t phase;
  i2c_controller_result_t result;
  bool busy;
  bool initialized;
} i2c_controller_t;

bool i2c_controller_init(
  i2c_controller_t *controller,
  volatile i2c0_registers_t *i2c
);
bool i2c_controller_start_write(
  i2c_controller_t *controller,
  uint8_t address,
  const uint8_t *data,
  size_t length,
  uint32_t now_ms
);
i2c_controller_result_t i2c_controller_poll(
  i2c_controller_t *controller,
  uint32_t now_ms
);
i2c_controller_result_t i2c_controller_take_result(
  i2c_controller_t *controller
);

#endif

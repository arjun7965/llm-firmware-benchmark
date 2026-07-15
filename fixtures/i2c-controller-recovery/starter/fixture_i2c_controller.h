#ifndef FIXTURE_I2C_CONTROLLER_H
#define FIXTURE_I2C_CONTROLLER_H

#include <stdint.h>

#define I2C0_BASE_ADDRESS UINT32_C(0x40005400)

#define I2C0_CONTROL_ENABLE UINT32_C(1)
#define I2C0_CONTROL_START (UINT32_C(1) << 1)
#define I2C0_CONTROL_STOP (UINT32_C(1) << 2)

#define I2C0_STATUS_START UINT32_C(1)
#define I2C0_STATUS_ADDRESS_ACK (UINT32_C(1) << 1)
#define I2C0_STATUS_DATA_ACK (UINT32_C(1) << 2)
#define I2C0_STATUS_NACK (UINT32_C(1) << 3)
#define I2C0_STATUS_ARBITRATION_LOST (UINT32_C(1) << 4)
#define I2C0_STATUS_BUS_ERROR (UINT32_C(1) << 5)
#define I2C0_STATUS_ALL \
  (I2C0_STATUS_START | I2C0_STATUS_ADDRESS_ACK | I2C0_STATUS_DATA_ACK | \
    I2C0_STATUS_NACK | I2C0_STATUS_ARBITRATION_LOST | \
    I2C0_STATUS_BUS_ERROR)

typedef struct i2c0_registers i2c0_registers_t;

uint32_t i2c0_read_status(const volatile i2c0_registers_t *i2c);
void i2c0_write_control(volatile i2c0_registers_t *i2c, uint32_t value);
void i2c0_write_status_clear(
  volatile i2c0_registers_t *i2c,
  uint32_t value
);
void i2c0_write_data(volatile i2c0_registers_t *i2c, uint8_t value);

#endif

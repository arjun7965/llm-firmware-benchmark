#ifndef MODBUS_RTU_RECEIVER_H
#define MODBUS_RTU_RECEIVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MODBUS_RTU_REQUEST_BYTES 8u
#define MODBUS_RTU_SILENCE_TICKS 4u
#define MODBUS_RTU_MIN_ADDRESS UINT8_C(1)
#define MODBUS_RTU_MAX_ADDRESS UINT8_C(247)
#define MODBUS_RTU_FUNCTION_READ_HOLDING UINT8_C(0x03)
#define MODBUS_RTU_FUNCTION_WRITE_SINGLE_REGISTER UINT8_C(0x06)
#define MODBUS_RTU_MAX_READ_REGISTERS UINT16_C(125)

typedef enum {
  MODBUS_RTU_RESULT_NONE = 0,
  MODBUS_RTU_RESULT_COMPLETE,
  MODBUS_RTU_RESULT_TIMEOUT,
  MODBUS_RTU_RESULT_BAD_CRC,
  MODBUS_RTU_RESULT_MALFORMED
} modbus_rtu_result_t;

typedef struct {
  uint8_t unit_address;
  uint8_t function;
  uint16_t register_address;
  uint16_t value;
} modbus_rtu_request_t;

typedef struct {
  uint8_t frame[MODBUS_RTU_REQUEST_BYTES];
  size_t frame_length;
  uint32_t last_byte_at;
  modbus_rtu_request_t request;
  modbus_rtu_result_t result;
  bool initialized;
  bool receiving;
} modbus_rtu_receiver_t;

bool modbus_rtu_receiver_init(modbus_rtu_receiver_t *receiver);
bool modbus_rtu_receiver_push_byte(
  modbus_rtu_receiver_t *receiver,
  uint8_t byte,
  uint32_t now_ticks
);
modbus_rtu_result_t modbus_rtu_receiver_poll(
  modbus_rtu_receiver_t *receiver,
  uint32_t now_ticks
);
modbus_rtu_result_t modbus_rtu_receiver_take_request(
  modbus_rtu_receiver_t *receiver,
  modbus_rtu_request_t *request
);

#endif

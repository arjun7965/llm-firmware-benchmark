#include "modbus_rtu_receiver.h"

static uint16_t crc16Modbus(const uint8_t *data, size_t length) {
  uint16_t crc = UINT16_C(0xFFFF);

  for (size_t index = 0u; index < length; index++) {
    crc ^= (uint16_t)data[index];
    for (uint8_t bit = 0u; bit < 8u; bit++) {
      crc = (crc & UINT16_C(1)) != 0u
        ? (uint16_t)((crc >> 1u) ^ UINT16_C(0xA001))
        : (uint16_t)(crc >> 1u);
    }
  }
  return crc;
}

static uint16_t readBigEndian16(const uint8_t *data) {
  return (uint16_t)(
    ((uint16_t)data[0] << 8u) |
    (uint16_t)data[1]
  );
}

static void clearRequest(modbus_rtu_request_t *request) {
  request->unit_address = 0u;
  request->function = 0u;
  request->register_address = 0u;
  request->value = 0u;
}

static void finish(
  modbus_rtu_receiver_t *receiver,
  modbus_rtu_result_t result
) {
  receiver->frame_length = 0u;
  receiver->receiving = false;
  receiver->result = result;
}

bool modbus_rtu_receiver_init(modbus_rtu_receiver_t *receiver) {
  if (receiver == NULL) return false;

  *receiver = (modbus_rtu_receiver_t){ 0 };
  receiver->initialized = true;
  return true;
}

bool modbus_rtu_receiver_push_byte(
  modbus_rtu_receiver_t *receiver,
  uint8_t byte,
  uint32_t now_ticks
) {
  if (
    receiver == NULL ||
    !receiver->initialized ||
    receiver->result != MODBUS_RTU_RESULT_NONE
  ) {
    return false;
  }
  if (
    receiver->receiving &&
    (uint32_t)(now_ticks - receiver->last_byte_at) >=
      MODBUS_RTU_SILENCE_TICKS
  ) {
    return false;
  }
  if (!receiver->receiving) {
    receiver->frame_length = 0u;
    receiver->receiving = true;
  }
  if (receiver->frame_length >= MODBUS_RTU_REQUEST_BYTES) {
    finish(receiver, MODBUS_RTU_RESULT_MALFORMED);
    return true;
  }

  receiver->frame[receiver->frame_length] = byte;
  receiver->frame_length++;
  receiver->last_byte_at = now_ticks;
  return true;
}

modbus_rtu_result_t modbus_rtu_receiver_poll(
  modbus_rtu_receiver_t *receiver,
  uint32_t now_ticks
) {
  if (
    receiver == NULL ||
    !receiver->initialized ||
    !receiver->receiving ||
    receiver->result != MODBUS_RTU_RESULT_NONE
  ) {
    return MODBUS_RTU_RESULT_NONE;
  }

  const uint32_t elapsed = (uint32_t)(now_ticks - receiver->last_byte_at);
  if (elapsed < MODBUS_RTU_SILENCE_TICKS) {
    return MODBUS_RTU_RESULT_NONE;
  }
  if (receiver->frame_length != MODBUS_RTU_REQUEST_BYTES) {
    finish(receiver, MODBUS_RTU_RESULT_TIMEOUT);
    return receiver->result;
  }

  const uint16_t encoded_crc = (uint16_t)(
    (uint16_t)receiver->frame[6] |
    ((uint16_t)receiver->frame[7] << 8u)
  );
  const uint16_t calculated_crc = crc16Modbus(receiver->frame, 6u);
  if (encoded_crc != calculated_crc) {
    finish(receiver, MODBUS_RTU_RESULT_BAD_CRC);
    return receiver->result;
  }

  const uint8_t unit_address = receiver->frame[0];
  const uint8_t function = receiver->frame[1];
  const uint16_t value = readBigEndian16(&receiver->frame[4]);
  if (
    unit_address < MODBUS_RTU_MIN_ADDRESS ||
    unit_address > MODBUS_RTU_MAX_ADDRESS ||
    (
      function != MODBUS_RTU_FUNCTION_READ_HOLDING &&
      function != MODBUS_RTU_FUNCTION_WRITE_SINGLE_REGISTER
    ) ||
    (
      function == MODBUS_RTU_FUNCTION_READ_HOLDING &&
      (value == 0u || value > MODBUS_RTU_MAX_READ_REGISTERS)
    )
  ) {
    finish(receiver, MODBUS_RTU_RESULT_MALFORMED);
    return receiver->result;
  }

  receiver->request.unit_address = unit_address;
  receiver->request.function = function;
  receiver->request.register_address = readBigEndian16(&receiver->frame[2]);
  receiver->request.value = value;
  finish(receiver, MODBUS_RTU_RESULT_COMPLETE);
  return receiver->result;
}

modbus_rtu_result_t modbus_rtu_receiver_take_request(
  modbus_rtu_receiver_t *receiver,
  modbus_rtu_request_t *request
) {
  if (request == NULL) return MODBUS_RTU_RESULT_NONE;
  clearRequest(request);
  if (
    receiver == NULL ||
    !receiver->initialized ||
    receiver->result == MODBUS_RTU_RESULT_NONE
  ) {
    return MODBUS_RTU_RESULT_NONE;
  }

  const modbus_rtu_result_t result = receiver->result;
  if (result == MODBUS_RTU_RESULT_COMPLETE) {
    *request = receiver->request;
  }
  clearRequest(&receiver->request);
  receiver->result = MODBUS_RTU_RESULT_NONE;
  return result;
}

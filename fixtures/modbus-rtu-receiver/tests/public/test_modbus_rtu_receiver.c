#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "modbus_rtu_receiver.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
              __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

static uint16_t testCrc16Modbus(const uint8_t *data, size_t length) {
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

static void buildRequest(
  uint8_t frame[MODBUS_RTU_REQUEST_BYTES],
  uint8_t unit_address,
  uint8_t function,
  uint16_t register_address,
  uint16_t value
) {
  frame[0] = unit_address;
  frame[1] = function;
  frame[2] = (uint8_t)(register_address >> 8u);
  frame[3] = (uint8_t)register_address;
  frame[4] = (uint8_t)(value >> 8u);
  frame[5] = (uint8_t)value;
  const uint16_t crc = testCrc16Modbus(frame, 6u);
  frame[6] = (uint8_t)crc;
  frame[7] = (uint8_t)(crc >> 8u);
}

static bool feedFrame(
  modbus_rtu_receiver_t *receiver,
  const uint8_t frame[MODBUS_RTU_REQUEST_BYTES],
  uint32_t started_at
) {
  for (size_t index = 0u; index < MODBUS_RTU_REQUEST_BYTES; index++) {
    if (!modbus_rtu_receiver_push_byte(
      receiver,
      frame[index],
      started_at + (uint32_t)index
    )) {
      return false;
    }
  }
  return true;
}

static bool test_read_request_and_result_gate(void) {
  modbus_rtu_receiver_t receiver;
  modbus_rtu_request_t request = {
    UINT8_C(0xFF),
    UINT8_C(0xFF),
    UINT16_C(0xFFFF),
    UINT16_C(0xFFFF),
  };
  uint8_t frame[MODBUS_RTU_REQUEST_BYTES];

  buildRequest(
    frame,
    MODBUS_RTU_MIN_ADDRESS,
    MODBUS_RTU_FUNCTION_READ_HOLDING,
    UINT16_C(0x0020),
    UINT16_C(1)
  );
  CHECK(modbus_rtu_receiver_init(&receiver));
  CHECK(feedFrame(&receiver, frame, 100u));
  CHECK(modbus_rtu_receiver_poll(&receiver, 110u) == MODBUS_RTU_RESULT_NONE);
  CHECK(
    modbus_rtu_receiver_poll(&receiver, 111u) ==
      MODBUS_RTU_RESULT_COMPLETE
  );
  CHECK(!modbus_rtu_receiver_push_byte(&receiver, 0u, 112u));
  CHECK(
    modbus_rtu_receiver_take_request(&receiver, &request) ==
      MODBUS_RTU_RESULT_COMPLETE
  );
  CHECK(request.unit_address == MODBUS_RTU_MIN_ADDRESS);
  CHECK(request.function == MODBUS_RTU_FUNCTION_READ_HOLDING);
  CHECK(request.register_address == UINT16_C(0x0020));
  CHECK(request.value == UINT16_C(1));
  CHECK(
    modbus_rtu_receiver_take_request(&receiver, &request) ==
      MODBUS_RTU_RESULT_NONE
  );
  CHECK(request.unit_address == 0u);
  CHECK(request.function == 0u);
  CHECK(request.register_address == 0u);
  CHECK(request.value == 0u);
  return true;
}

static bool test_write_request_across_tick_wrap(void) {
  modbus_rtu_receiver_t receiver;
  modbus_rtu_request_t request;
  uint8_t frame[MODBUS_RTU_REQUEST_BYTES];
  const uint32_t started_at = UINT32_MAX - UINT32_C(3);

  buildRequest(
    frame,
    UINT8_C(17),
    MODBUS_RTU_FUNCTION_WRITE_SINGLE_REGISTER,
    UINT16_C(0x1234),
    UINT16_C(0xBEEF)
  );
  CHECK(modbus_rtu_receiver_init(&receiver));
  CHECK(feedFrame(&receiver, frame, started_at));
  CHECK(
    modbus_rtu_receiver_poll(&receiver, started_at + UINT32_C(10)) ==
      MODBUS_RTU_RESULT_NONE
  );
  CHECK(
    modbus_rtu_receiver_poll(&receiver, started_at + UINT32_C(11)) ==
      MODBUS_RTU_RESULT_COMPLETE
  );
  CHECK(
    modbus_rtu_receiver_take_request(&receiver, &request) ==
      MODBUS_RTU_RESULT_COMPLETE
  );
  CHECK(request.unit_address == UINT8_C(17));
  CHECK(request.function == MODBUS_RTU_FUNCTION_WRITE_SINGLE_REGISTER);
  CHECK(request.register_address == UINT16_C(0x1234));
  CHECK(request.value == UINT16_C(0xBEEF));
  return true;
}

static bool test_incomplete_timeout_requires_explicit_poll(void) {
  modbus_rtu_receiver_t receiver;
  modbus_rtu_request_t request = {
    UINT8_C(1),
    UINT8_C(1),
    UINT16_C(1),
    UINT16_C(1),
  };
  uint8_t frame[MODBUS_RTU_REQUEST_BYTES];

  CHECK(modbus_rtu_receiver_init(&receiver));
  CHECK(modbus_rtu_receiver_push_byte(&receiver, UINT8_C(1), 10u));
  CHECK(!modbus_rtu_receiver_push_byte(&receiver, UINT8_C(3), 14u));
  CHECK(
    modbus_rtu_receiver_poll(&receiver, 14u) ==
      MODBUS_RTU_RESULT_TIMEOUT
  );
  CHECK(
    modbus_rtu_receiver_take_request(&receiver, &request) ==
      MODBUS_RTU_RESULT_TIMEOUT
  );
  CHECK(request.unit_address == 0u);
  CHECK(request.function == 0u);
  CHECK(request.register_address == 0u);
  CHECK(request.value == 0u);

  buildRequest(
    frame,
    UINT8_C(2),
    MODBUS_RTU_FUNCTION_READ_HOLDING,
    UINT16_C(0),
    UINT16_C(2)
  );
  CHECK(feedFrame(&receiver, frame, 20u));
  CHECK(
    modbus_rtu_receiver_poll(&receiver, 31u) ==
      MODBUS_RTU_RESULT_COMPLETE
  );
  CHECK(
    modbus_rtu_receiver_take_request(&receiver, &request) ==
      MODBUS_RTU_RESULT_COMPLETE
  );
  return true;
}

static bool test_bad_crc_and_malformed_requests_recover(void) {
  modbus_rtu_receiver_t receiver;
  modbus_rtu_request_t request;
  uint8_t frame[MODBUS_RTU_REQUEST_BYTES];

  CHECK(modbus_rtu_receiver_init(&receiver));
  buildRequest(
    frame,
    UINT8_C(3),
    MODBUS_RTU_FUNCTION_READ_HOLDING,
    UINT16_C(4),
    UINT16_C(2)
  );
  frame[6] ^= UINT8_C(1);
  CHECK(feedFrame(&receiver, frame, 0u));
  CHECK(modbus_rtu_receiver_poll(&receiver, 11u) == MODBUS_RTU_RESULT_BAD_CRC);
  CHECK(
    modbus_rtu_receiver_take_request(&receiver, &request) ==
      MODBUS_RTU_RESULT_BAD_CRC
  );

  buildRequest(
    frame,
    UINT8_C(0),
    MODBUS_RTU_FUNCTION_READ_HOLDING,
    UINT16_C(4),
    UINT16_C(2)
  );
  CHECK(feedFrame(&receiver, frame, 20u));
  CHECK(
    modbus_rtu_receiver_poll(&receiver, 31u) ==
      MODBUS_RTU_RESULT_MALFORMED
  );
  CHECK(
    modbus_rtu_receiver_take_request(&receiver, &request) ==
      MODBUS_RTU_RESULT_MALFORMED
  );

  buildRequest(
    frame,
    UINT8_C(3),
    MODBUS_RTU_FUNCTION_READ_HOLDING,
    UINT16_C(4),
    UINT16_C(0)
  );
  CHECK(feedFrame(&receiver, frame, 40u));
  CHECK(
    modbus_rtu_receiver_poll(&receiver, 51u) ==
      MODBUS_RTU_RESULT_MALFORMED
  );
  CHECK(
    modbus_rtu_receiver_take_request(&receiver, &request) ==
      MODBUS_RTU_RESULT_MALFORMED
  );
  return true;
}

static bool test_oversize_frame_is_terminal_and_recoverable(void) {
  modbus_rtu_receiver_t receiver;
  modbus_rtu_request_t request;
  uint8_t frame[MODBUS_RTU_REQUEST_BYTES];

  CHECK(modbus_rtu_receiver_init(&receiver));
  memset(frame, 0, sizeof(frame));
  for (size_t index = 0u; index < sizeof(frame); index++) {
    CHECK(modbus_rtu_receiver_push_byte(&receiver, frame[index], (uint32_t)index));
  }
  CHECK(modbus_rtu_receiver_push_byte(&receiver, UINT8_C(0xA5), 8u));
  CHECK(!modbus_rtu_receiver_push_byte(&receiver, UINT8_C(0x5A), 9u));
  CHECK(
    modbus_rtu_receiver_take_request(&receiver, &request) ==
      MODBUS_RTU_RESULT_MALFORMED
  );

  buildRequest(
    frame,
    UINT8_C(4),
    MODBUS_RTU_FUNCTION_WRITE_SINGLE_REGISTER,
    UINT16_C(7),
    UINT16_C(9)
  );
  CHECK(feedFrame(&receiver, frame, 20u));
  CHECK(
    modbus_rtu_receiver_poll(&receiver, 31u) ==
      MODBUS_RTU_RESULT_COMPLETE
  );
  CHECK(
    modbus_rtu_receiver_take_request(&receiver, &request) ==
      MODBUS_RTU_RESULT_COMPLETE
  );
  return true;
}

static bool test_null_and_uninitialized_arguments(void) {
  modbus_rtu_receiver_t receiver = { 0 };
  modbus_rtu_request_t request = {
    UINT8_C(1),
    UINT8_C(1),
    UINT16_C(1),
    UINT16_C(1),
  };

  CHECK(!modbus_rtu_receiver_init(NULL));
  CHECK(!modbus_rtu_receiver_push_byte(&receiver, 0u, 0u));
  CHECK(modbus_rtu_receiver_poll(&receiver, 0u) == MODBUS_RTU_RESULT_NONE);
  CHECK(
    modbus_rtu_receiver_take_request(&receiver, &request) ==
      MODBUS_RTU_RESULT_NONE
  );
  CHECK(request.unit_address == 0u);
  CHECK(request.function == 0u);
  CHECK(request.register_address == 0u);
  CHECK(request.value == 0u);
  CHECK(modbus_rtu_receiver_take_request(&receiver, NULL) == MODBUS_RTU_RESULT_NONE);
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "read request and result gate", test_read_request_and_result_gate },
    { "write request across tick wrap", test_write_request_across_tick_wrap },
    { "incomplete timeout", test_incomplete_timeout_requires_explicit_poll },
    { "bad CRC and malformed recovery", test_bad_crc_and_malformed_requests_recover },
    { "oversize frame recovery", test_oversize_frame_is_terminal_and_recoverable },
    { "null and uninitialized arguments", test_null_and_uninitialized_arguments },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) return 1;
    printf("ok - %s\n", tests[index].name);
  }
  return 0;
}

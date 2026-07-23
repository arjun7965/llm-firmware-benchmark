#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "can_transport_reassembly.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
              __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

static void clearFrame(can_transport_frame_t *frame) {
  *frame = (can_transport_frame_t){ 0 };
  frame->identifier = CAN_TRANSPORT_DATA_ID;
}

static void buildSingle(
  can_transport_frame_t *frame,
  const uint8_t *payload,
  size_t payload_length
) {
  clearFrame(frame);
  frame->dlc = (uint8_t)(payload_length + 1u);
  frame->data[0] = (uint8_t)payload_length;
  for (size_t index = 0u; index < payload_length; index++) {
    frame->data[index + 1u] = payload[index];
  }
}

static void buildFirst(
  can_transport_frame_t *frame,
  const uint8_t *payload,
  size_t payload_length
) {
  clearFrame(frame);
  frame->dlc = CAN_TRANSPORT_FRAME_BYTES;
  frame->data[0] = (uint8_t)(
    UINT8_C(0x10) | (uint8_t)(payload_length >> 8u)
  );
  frame->data[1] = (uint8_t)payload_length;
  for (size_t index = 0u;
       index < CAN_TRANSPORT_FIRST_FRAME_DATA_BYTES;
       index++) {
    frame->data[index + 2u] = payload[index];
  }
}

static void buildConsecutive(
  can_transport_frame_t *frame,
  uint8_t sequence,
  const uint8_t *payload,
  size_t payload_length
) {
  clearFrame(frame);
  frame->dlc = (uint8_t)(payload_length + 1u);
  frame->data[0] = (uint8_t)(UINT8_C(0x20) | sequence);
  for (size_t index = 0u; index < payload_length; index++) {
    frame->data[index + 1u] = payload[index];
  }
}

static bool test_single_frame_and_result_gate(void) {
  can_transport_receiver_t receiver;
  can_transport_message_t message = { { UINT8_C(0xFF) }, 99u };
  can_transport_frame_t frame;
  const uint8_t payload[] = { UINT8_C(0xA0), UINT8_C(0xB1), UINT8_C(0xC2) };

  buildSingle(&frame, payload, sizeof(payload));
  CHECK(can_transport_receiver_init(&receiver));
  CHECK(can_transport_receiver_accept(&receiver, &frame, 10u));
  CHECK(!can_transport_receiver_accept(&receiver, &frame, 11u));
  CHECK(
    can_transport_receiver_take_message(&receiver, &message) ==
      CAN_TRANSPORT_RESULT_COMPLETE
  );
  CHECK(message.length == sizeof(payload));
  CHECK(memcmp(message.data, payload, sizeof(payload)) == 0);
  CHECK(
    can_transport_receiver_take_message(&receiver, &message) ==
      CAN_TRANSPORT_RESULT_NONE
  );
  CHECK(message.length == 0u);
  return true;
}

static bool test_multiframe_sequence_and_payload(void) {
  can_transport_receiver_t receiver;
  can_transport_message_t message;
  can_transport_frame_t frame;
  uint8_t payload[20];
  const uint32_t started_at = UINT32_MAX - UINT32_C(3);

  for (size_t index = 0u; index < sizeof(payload); index++) {
    payload[index] = (uint8_t)(UINT8_C(0x40) + index);
  }
  CHECK(can_transport_receiver_init(&receiver));
  buildFirst(&frame, payload, sizeof(payload));
  CHECK(can_transport_receiver_accept(&receiver, &frame, started_at));
  buildConsecutive(
    &frame,
    UINT8_C(1),
    &payload[CAN_TRANSPORT_FIRST_FRAME_DATA_BYTES],
    CAN_TRANSPORT_CONSECUTIVE_FRAME_DATA_BYTES
  );
  CHECK(
    can_transport_receiver_accept(&receiver, &frame, started_at + UINT32_C(2))
  );
  buildConsecutive(
    &frame,
    UINT8_C(2),
    &payload[
      CAN_TRANSPORT_FIRST_FRAME_DATA_BYTES +
      CAN_TRANSPORT_CONSECUTIVE_FRAME_DATA_BYTES
    ],
    CAN_TRANSPORT_CONSECUTIVE_FRAME_DATA_BYTES
  );
  CHECK(
    can_transport_receiver_accept(&receiver, &frame, started_at + UINT32_C(4))
  );
  CHECK(
    can_transport_receiver_take_message(&receiver, &message) ==
      CAN_TRANSPORT_RESULT_COMPLETE
  );
  CHECK(message.length == sizeof(payload));
  CHECK(memcmp(message.data, payload, sizeof(payload)) == 0);
  return true;
}

static bool test_first_frame_minimum_and_sequence_recovery(void) {
  can_transport_receiver_t receiver;
  can_transport_message_t message;
  can_transport_frame_t frame;
  const uint8_t payload[] = {
    UINT8_C(1), UINT8_C(2), UINT8_C(3), UINT8_C(4),
    UINT8_C(5), UINT8_C(6), UINT8_C(7), UINT8_C(8),
  };

  CHECK(can_transport_receiver_init(&receiver));
  buildFirst(&frame, payload, sizeof(payload));
  CHECK(can_transport_receiver_accept(&receiver, &frame, 0u));
  buildConsecutive(&frame, UINT8_C(2), &payload[6], 2u);
  CHECK(can_transport_receiver_accept(&receiver, &frame, 1u));
  CHECK(
    can_transport_receiver_take_message(&receiver, &message) ==
      CAN_TRANSPORT_RESULT_MALFORMED
  );

  buildSingle(&frame, payload, 2u);
  CHECK(can_transport_receiver_accept(&receiver, &frame, 2u));
  CHECK(
    can_transport_receiver_take_message(&receiver, &message) ==
      CAN_TRANSPORT_RESULT_COMPLETE
  );
  CHECK(message.length == 2u);
  CHECK(message.data[0] == UINT8_C(1));
  CHECK(message.data[1] == UINT8_C(2));
  return true;
}

static bool test_source_and_dlc_malformed_recovery(void) {
  can_transport_receiver_t receiver;
  can_transport_message_t message = { { UINT8_C(0xFF) }, 99u };
  can_transport_frame_t frame;
  const uint8_t payload[] = { UINT8_C(9), UINT8_C(8), UINT8_C(7) };

  CHECK(can_transport_receiver_init(&receiver));
  buildSingle(&frame, payload, sizeof(payload));
  frame.identifier = UINT16_C(0x322);
  CHECK(can_transport_receiver_accept(&receiver, &frame, 0u));
  CHECK(
    can_transport_receiver_take_message(&receiver, &message) ==
      CAN_TRANSPORT_RESULT_MALFORMED
  );
  CHECK(message.length == 0u);

  buildSingle(&frame, payload, sizeof(payload));
  frame.dlc = 2u;
  CHECK(can_transport_receiver_accept(&receiver, &frame, 1u));
  CHECK(
    can_transport_receiver_take_message(&receiver, &message) ==
      CAN_TRANSPORT_RESULT_MALFORMED
  );

  buildSingle(&frame, payload, sizeof(payload));
  CHECK(can_transport_receiver_accept(&receiver, &frame, 2u));
  CHECK(
    can_transport_receiver_take_message(&receiver, &message) ==
      CAN_TRANSPORT_RESULT_COMPLETE
  );
  return true;
}

static bool test_timeout_is_wrap_safe_and_terminal(void) {
  can_transport_receiver_t receiver;
  can_transport_message_t message;
  can_transport_frame_t frame;
  const uint8_t payload[] = {
    UINT8_C(1), UINT8_C(2), UINT8_C(3), UINT8_C(4),
    UINT8_C(5), UINT8_C(6), UINT8_C(7), UINT8_C(8),
    UINT8_C(9),
  };
  const uint32_t started_at = UINT32_MAX - UINT32_C(5);

  CHECK(can_transport_receiver_init(&receiver));
  buildFirst(&frame, payload, sizeof(payload));
  CHECK(can_transport_receiver_accept(&receiver, &frame, started_at));
  CHECK(
    can_transport_receiver_poll(&receiver, started_at + UINT32_C(9)) ==
      CAN_TRANSPORT_RESULT_NONE
  );
  CHECK(
    can_transport_receiver_poll(&receiver, started_at + UINT32_C(10)) ==
      CAN_TRANSPORT_RESULT_TIMEOUT
  );
  CHECK(
    can_transport_receiver_take_message(&receiver, &message) ==
      CAN_TRANSPORT_RESULT_TIMEOUT
  );
  CHECK(message.length == 0u);
  return true;
}

static bool test_null_and_uninitialized_arguments(void) {
  can_transport_receiver_t receiver = { 0 };
  can_transport_message_t message = { { UINT8_C(0xFF) }, 99u };
  can_transport_frame_t frame;
  const uint8_t payload[] = { UINT8_C(1) };

  buildSingle(&frame, payload, sizeof(payload));
  CHECK(!can_transport_receiver_init(NULL));
  CHECK(!can_transport_receiver_accept(&receiver, &frame, 0u));
  CHECK(can_transport_receiver_poll(&receiver, 0u) == CAN_TRANSPORT_RESULT_NONE);
  CHECK(
    can_transport_receiver_take_message(&receiver, &message) ==
      CAN_TRANSPORT_RESULT_NONE
  );
  CHECK(message.length == 0u);
  CHECK(can_transport_receiver_take_message(&receiver, NULL) == CAN_TRANSPORT_RESULT_NONE);
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "single frame and result gate", test_single_frame_and_result_gate },
    { "multiframe sequence and payload", test_multiframe_sequence_and_payload },
    { "first frame minimum and sequence recovery", test_first_frame_minimum_and_sequence_recovery },
    { "source and DLC malformed recovery", test_source_and_dlc_malformed_recovery },
    { "wrap-safe timeout", test_timeout_is_wrap_safe_and_terminal },
    { "null and uninitialized arguments", test_null_and_uninitialized_arguments },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) return 1;
    printf("ok - %s\n", tests[index].name);
  }
  return 0;
}

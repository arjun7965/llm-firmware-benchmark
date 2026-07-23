#ifndef CAN_TRANSPORT_REASSEMBLY_H
#define CAN_TRANSPORT_REASSEMBLY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CAN_TRANSPORT_DATA_ID UINT16_C(0x321)
#define CAN_TRANSPORT_FRAME_BYTES 8u
#define CAN_TRANSPORT_FIRST_FRAME_DATA_BYTES 6u
#define CAN_TRANSPORT_CONSECUTIVE_FRAME_DATA_BYTES 7u
#define CAN_TRANSPORT_MAX_MESSAGE_BYTES 24u
#define CAN_TRANSPORT_TIMEOUT_MS 10u

typedef enum {
  CAN_TRANSPORT_RESULT_NONE = 0,
  CAN_TRANSPORT_RESULT_COMPLETE,
  CAN_TRANSPORT_RESULT_TIMEOUT,
  CAN_TRANSPORT_RESULT_MALFORMED
} can_transport_result_t;

typedef struct {
  uint16_t identifier;
  uint8_t dlc;
  uint8_t data[CAN_TRANSPORT_FRAME_BYTES];
} can_transport_frame_t;

typedef struct {
  uint8_t data[CAN_TRANSPORT_MAX_MESSAGE_BYTES];
  size_t length;
} can_transport_message_t;

typedef struct {
  uint8_t buffer[CAN_TRANSPORT_MAX_MESSAGE_BYTES];
  size_t expected_length;
  size_t received_length;
  uint8_t next_sequence;
  uint32_t last_frame_at;
  can_transport_message_t message;
  can_transport_result_t result;
  bool initialized;
  bool receiving;
} can_transport_receiver_t;

bool can_transport_receiver_init(can_transport_receiver_t *receiver);
bool can_transport_receiver_accept(
  can_transport_receiver_t *receiver,
  const can_transport_frame_t *frame,
  uint32_t now_ms
);
can_transport_result_t can_transport_receiver_poll(
  can_transport_receiver_t *receiver,
  uint32_t now_ms
);
can_transport_result_t can_transport_receiver_take_message(
  can_transport_receiver_t *receiver,
  can_transport_message_t *message
);

#endif

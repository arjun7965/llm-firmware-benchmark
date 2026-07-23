#include "can_transport_reassembly.h"

static void clearMessage(can_transport_message_t *message) {
  message->length = 0u;
  for (size_t index = 0u; index < CAN_TRANSPORT_MAX_MESSAGE_BYTES; index++) {
    message->data[index] = 0u;
  }
}

static void finish(
  can_transport_receiver_t *receiver,
  can_transport_result_t result
) {
  receiver->expected_length = 0u;
  receiver->received_length = 0u;
  receiver->next_sequence = 0u;
  receiver->receiving = false;
  receiver->result = result;
}

static void publishMessage(can_transport_receiver_t *receiver) {
  clearMessage(&receiver->message);
  receiver->message.length = receiver->received_length;
  for (size_t index = 0u; index < receiver->received_length; index++) {
    receiver->message.data[index] = receiver->buffer[index];
  }
  finish(receiver, CAN_TRANSPORT_RESULT_COMPLETE);
}

static void malformed(can_transport_receiver_t *receiver) {
  finish(receiver, CAN_TRANSPORT_RESULT_MALFORMED);
}

bool can_transport_receiver_init(can_transport_receiver_t *receiver) {
  if (receiver == NULL) return false;

  *receiver = (can_transport_receiver_t){ 0 };
  receiver->initialized = true;
  return true;
}

bool can_transport_receiver_accept(
  can_transport_receiver_t *receiver,
  const can_transport_frame_t *frame,
  uint32_t now_ms
) {
  if (
    receiver == NULL ||
    frame == NULL ||
    !receiver->initialized ||
    receiver->result != CAN_TRANSPORT_RESULT_NONE
  ) {
    return false;
  }
  if (
    receiver->receiving &&
    (uint32_t)(now_ms - receiver->last_frame_at) >=
      CAN_TRANSPORT_TIMEOUT_MS
  ) {
    return false;
  }
  if (
    frame->identifier != CAN_TRANSPORT_DATA_ID ||
    frame->dlc == 0u ||
    frame->dlc > CAN_TRANSPORT_FRAME_BYTES
  ) {
    malformed(receiver);
    return true;
  }

  const uint8_t frame_type = (uint8_t)(frame->data[0] >> 4u);
  if (frame_type == 0u) {
    const size_t payload_length = (size_t)(frame->data[0] & UINT8_C(0x0F));
    const uint8_t expected_dlc = (uint8_t)(payload_length + 1u);
    if (receiver->receiving || frame->dlc != expected_dlc) {
      malformed(receiver);
      return true;
    }
    clearMessage(&receiver->message);
    receiver->message.length = payload_length;
    for (size_t index = 0u; index < payload_length; index++) {
      receiver->message.data[index] = frame->data[index + 1u];
    }
    finish(receiver, CAN_TRANSPORT_RESULT_COMPLETE);
    return true;
  }

  if (frame_type == UINT8_C(1)) {
    const size_t declared_length = (size_t)(
      ((uint16_t)(frame->data[0] & UINT8_C(0x0F)) << 8u) |
      (uint16_t)frame->data[1]
    );
    if (
      receiver->receiving ||
      frame->dlc != CAN_TRANSPORT_FRAME_BYTES ||
      declared_length < 8u ||
      declared_length > CAN_TRANSPORT_MAX_MESSAGE_BYTES
    ) {
      malformed(receiver);
      return true;
    }

    for (size_t index = 0u;
         index < CAN_TRANSPORT_FIRST_FRAME_DATA_BYTES;
         index++) {
      receiver->buffer[index] = frame->data[index + 2u];
    }
    receiver->expected_length = declared_length;
    receiver->received_length = CAN_TRANSPORT_FIRST_FRAME_DATA_BYTES;
    receiver->next_sequence = UINT8_C(1);
    receiver->last_frame_at = now_ms;
    receiver->receiving = true;
    return true;
  }

  if (frame_type == UINT8_C(2)) {
    if (!receiver->receiving) {
      malformed(receiver);
      return true;
    }

    const uint8_t sequence = (uint8_t)(frame->data[0] & UINT8_C(0x0F));
    if (sequence != receiver->next_sequence) {
      malformed(receiver);
      return true;
    }
    const size_t remaining = receiver->expected_length - receiver->received_length;
    const size_t payload_length = remaining <
      CAN_TRANSPORT_CONSECUTIVE_FRAME_DATA_BYTES
      ? remaining
      : CAN_TRANSPORT_CONSECUTIVE_FRAME_DATA_BYTES;
    const uint8_t expected_dlc = (uint8_t)(payload_length + 1u);
    if (frame->dlc != expected_dlc) {
      malformed(receiver);
      return true;
    }

    for (size_t index = 0u; index < payload_length; index++) {
      receiver->buffer[receiver->received_length + index] =
        frame->data[index + 1u];
    }
    receiver->received_length += payload_length;
    receiver->next_sequence = (uint8_t)(
      (receiver->next_sequence + UINT8_C(1)) & UINT8_C(0x0F)
    );
    receiver->last_frame_at = now_ms;
    if (receiver->received_length == receiver->expected_length) {
      publishMessage(receiver);
    }
    return true;
  }

  malformed(receiver);
  return true;
}

can_transport_result_t can_transport_receiver_poll(
  can_transport_receiver_t *receiver,
  uint32_t now_ms
) {
  if (
    receiver == NULL ||
    !receiver->initialized ||
    !receiver->receiving ||
    receiver->result != CAN_TRANSPORT_RESULT_NONE
  ) {
    return CAN_TRANSPORT_RESULT_NONE;
  }

  const uint32_t elapsed = (uint32_t)(now_ms - receiver->last_frame_at);
  if (elapsed < CAN_TRANSPORT_TIMEOUT_MS) {
    return CAN_TRANSPORT_RESULT_NONE;
  }
  finish(receiver, CAN_TRANSPORT_RESULT_TIMEOUT);
  return receiver->result;
}

can_transport_result_t can_transport_receiver_take_message(
  can_transport_receiver_t *receiver,
  can_transport_message_t *message
) {
  if (message == NULL) return CAN_TRANSPORT_RESULT_NONE;
  clearMessage(message);
  if (
    receiver == NULL ||
    !receiver->initialized ||
    receiver->result == CAN_TRANSPORT_RESULT_NONE
  ) {
    return CAN_TRANSPORT_RESULT_NONE;
  }

  const can_transport_result_t result = receiver->result;
  if (result == CAN_TRANSPORT_RESULT_COMPLETE) {
    *message = receiver->message;
  }
  clearMessage(&receiver->message);
  receiver->result = CAN_TRANSPORT_RESULT_NONE;
  return result;
}

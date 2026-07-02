#include "binary_parser.h"

#define FRAME_PREFIX_SIZE 5u
#define FRAME_CRC_SIZE 2u

static uint16_t crc16CcittFalse(
  const uint8_t *data,
  size_t length
) {
  uint16_t crc = UINT16_C(0xFFFF);
  for (size_t index = 0u; index < length; index++) {
    crc ^= (uint16_t)((uint16_t)data[index] << 8u);
    for (uint8_t bit = 0u; bit < 8u; bit++) {
      if ((crc & UINT16_C(0x8000)) != 0u) {
        crc = (uint16_t)((uint16_t)(crc << 1u) ^ UINT16_C(0x1021));
      } else {
        crc = (uint16_t)(crc << 1u);
      }
    }
  }
  return crc;
}

frame_parse_result_t frame_parse(
  const uint8_t *input,
  size_t inputLength,
  frame_payload_view_t *payload
) {
  if (payload == NULL) return FRAME_PARSE_NULL_ARGUMENT;
  payload->data = NULL;
  payload->length = 0u;
  if (input == NULL) return FRAME_PARSE_NULL_ARGUMENT;

  if (inputLength < FRAME_PREFIX_SIZE) return FRAME_PARSE_TRUNCATED;
  if (
    input[0] != FRAME_MAGIC_FIRST ||
    input[1] != FRAME_MAGIC_SECOND
  ) {
    return FRAME_PARSE_BAD_MAGIC;
  }
  if (input[2] != FRAME_VERSION) {
    return FRAME_PARSE_UNSUPPORTED_VERSION;
  }

  const uint16_t encodedLength = (uint16_t)(
    (uint16_t)input[3] |
    (uint16_t)((uint16_t)input[4] << 8u)
  );
  const size_t payloadLength = (size_t)encodedLength;
  if (payloadLength > FRAME_MAX_PAYLOAD) {
    return FRAME_PARSE_PAYLOAD_TOO_LARGE;
  }

  size_t expectedLength = FRAME_PREFIX_SIZE;
  if (payloadLength > SIZE_MAX - expectedLength) {
    return FRAME_PARSE_LENGTH_OVERFLOW;
  }
  expectedLength += payloadLength;
  if (FRAME_CRC_SIZE > SIZE_MAX - expectedLength) {
    return FRAME_PARSE_LENGTH_OVERFLOW;
  }
  expectedLength += FRAME_CRC_SIZE;

  if (inputLength < expectedLength) return FRAME_PARSE_TRUNCATED;
  if (inputLength > expectedLength) return FRAME_PARSE_TRAILING_DATA;

  const size_t crcOffset = FRAME_PREFIX_SIZE + payloadLength;
  const uint16_t encodedCrc = (uint16_t)(
    (uint16_t)input[crcOffset] |
    (uint16_t)((uint16_t)input[crcOffset + 1u] << 8u)
  );
  const uint16_t calculatedCrc = crc16CcittFalse(
    &input[2],
    3u + payloadLength
  );
  if (encodedCrc != calculatedCrc) return FRAME_PARSE_BAD_CRC;

  payload->data = &input[FRAME_PREFIX_SIZE];
  payload->length = payloadLength;
  return FRAME_PARSE_OK;
}

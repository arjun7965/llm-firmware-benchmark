#ifndef BINARY_PARSER_H
#define BINARY_PARSER_H

#include <stddef.h>
#include <stdint.h>

#define FRAME_MAGIC_FIRST UINT8_C(0xA5)
#define FRAME_MAGIC_SECOND UINT8_C(0x5A)
#define FRAME_VERSION UINT8_C(1)
#define FRAME_MAX_PAYLOAD 1024u

typedef enum {
  FRAME_PARSE_OK = 0,
  FRAME_PARSE_NULL_ARGUMENT,
  FRAME_PARSE_TRUNCATED,
  FRAME_PARSE_BAD_MAGIC,
  FRAME_PARSE_UNSUPPORTED_VERSION,
  FRAME_PARSE_PAYLOAD_TOO_LARGE,
  FRAME_PARSE_LENGTH_OVERFLOW,
  FRAME_PARSE_TRAILING_DATA,
  FRAME_PARSE_BAD_CRC
} frame_parse_result_t;

typedef struct {
  const uint8_t *data;
  size_t length;
} frame_payload_view_t;

frame_parse_result_t frame_parse(
  const uint8_t *input,
  size_t inputLength,
  frame_payload_view_t *payload
);

#endif

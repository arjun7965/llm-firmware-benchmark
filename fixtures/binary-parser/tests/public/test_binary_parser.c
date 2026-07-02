#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "binary_parser.h"

#define FRAME_OVERHEAD 7u

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
              __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

static uint16_t testCrc16CcittFalse(
  const uint8_t *data,
  size_t length
) {
  uint16_t crc = UINT16_C(0xFFFF);
  for (size_t index = 0u; index < length; index++) {
    crc ^= (uint16_t)((uint16_t)data[index] << 8u);
    for (uint8_t bit = 0u; bit < 8u; bit++) {
      crc = (crc & UINT16_C(0x8000)) != 0u
        ? (uint16_t)((uint16_t)(crc << 1u) ^ UINT16_C(0x1021))
        : (uint16_t)(crc << 1u);
    }
  }
  return crc;
}

static size_t buildFrame(
  uint8_t *frame,
  const uint8_t *payload,
  uint16_t payloadLength
) {
  frame[0] = FRAME_MAGIC_FIRST;
  frame[1] = FRAME_MAGIC_SECOND;
  frame[2] = FRAME_VERSION;
  frame[3] = (uint8_t)(payloadLength & UINT16_C(0x00FF));
  frame[4] = (uint8_t)(payloadLength >> 8u);
  if (payloadLength > 0u) {
    memcpy(&frame[5], payload, payloadLength);
  }
  const uint16_t crc = testCrc16CcittFalse(
    &frame[2],
    3u + (size_t)payloadLength
  );
  frame[5u + payloadLength] = (uint8_t)(crc & UINT16_C(0x00FF));
  frame[6u + payloadLength] = (uint8_t)(crc >> 8u);
  return (size_t)payloadLength + FRAME_OVERHEAD;
}

static bool test_valid_empty_payload(void) {
  uint8_t frame[FRAME_OVERHEAD];
  frame_payload_view_t payload = { NULL, 99u };
  const uint8_t check[] = {
    (uint8_t)'1',
    (uint8_t)'2',
    (uint8_t)'3',
    (uint8_t)'4',
    (uint8_t)'5',
    (uint8_t)'6',
    (uint8_t)'7',
    (uint8_t)'8',
    (uint8_t)'9',
  };
  CHECK(
    testCrc16CcittFalse(check, sizeof(check)) == UINT16_C(0x29B1)
  );
  const size_t frameLength = buildFrame(frame, NULL, 0u);
  CHECK(frame_parse(frame, frameLength, &payload) == FRAME_PARSE_OK);
  CHECK(payload.data == &frame[5]);
  CHECK(payload.length == 0u);
  return true;
}

static bool test_valid_unaligned_maximum_payload(void) {
  uint8_t storage[FRAME_MAX_PAYLOAD + FRAME_OVERHEAD + 1u];
  uint8_t source[FRAME_MAX_PAYLOAD];
  uint8_t *const frame = &storage[1];
  frame_payload_view_t payload;
  for (size_t index = 0u; index < sizeof(source); index++) {
    source[index] = (uint8_t)(index ^ (index >> 8u));
  }

  const size_t frameLength = buildFrame(
    frame,
    source,
    (uint16_t)sizeof(source)
  );
  CHECK(frame_parse(frame, frameLength, &payload) == FRAME_PARSE_OK);
  CHECK(payload.data == &frame[5]);
  CHECK(payload.length == sizeof(source));
  CHECK(memcmp(payload.data, source, sizeof(source)) == 0);
  return true;
}

static bool test_every_truncation_boundary(void) {
  const uint8_t source[] = {
    UINT8_C(0x10),
    UINT8_C(0x20),
    UINT8_C(0x30),
  };
  uint8_t frame[sizeof(source) + FRAME_OVERHEAD];
  const size_t frameLength = buildFrame(
    frame,
    source,
    (uint16_t)sizeof(source)
  );
  for (size_t length = 0u; length < frameLength; length++) {
    frame_payload_view_t payload = {
      frame,
      99u,
    };
    CHECK(
      frame_parse(frame, length, &payload) == FRAME_PARSE_TRUNCATED
    );
    CHECK(payload.data == NULL);
    CHECK(payload.length == 0u);
  }
  return true;
}

static bool test_rejects_trailing_data(void) {
  const uint8_t source[] = { UINT8_C(0xAA) };
  uint8_t frame[sizeof(source) + FRAME_OVERHEAD + 1u];
  frame_payload_view_t payload;
  const size_t frameLength = buildFrame(
    frame,
    source,
    (uint16_t)sizeof(source)
  );
  frame[frameLength] = 0u;
  CHECK(
    frame_parse(frame, frameLength + 1u, &payload) ==
      FRAME_PARSE_TRAILING_DATA
  );
  return true;
}

static bool test_rejects_bad_magic_and_version(void) {
  uint8_t frame[FRAME_OVERHEAD];
  frame_payload_view_t payload;
  const size_t frameLength = buildFrame(frame, NULL, 0u);

  frame[0] ^= UINT8_C(1);
  CHECK(
    frame_parse(frame, frameLength, &payload) == FRAME_PARSE_BAD_MAGIC
  );
  frame[0] = FRAME_MAGIC_FIRST;
  frame[1] ^= UINT8_C(1);
  CHECK(
    frame_parse(frame, frameLength, &payload) == FRAME_PARSE_BAD_MAGIC
  );
  frame[1] = FRAME_MAGIC_SECOND;
  frame[2] = UINT8_C(2);
  CHECK(
    frame_parse(frame, frameLength, &payload) ==
      FRAME_PARSE_UNSUPPORTED_VERSION
  );
  return true;
}

static bool test_rejects_oversized_payload(void) {
  uint8_t header[5] = {
    FRAME_MAGIC_FIRST,
    FRAME_MAGIC_SECOND,
    FRAME_VERSION,
    UINT8_C(0x01),
    UINT8_C(0x04),
  };
  frame_payload_view_t payload;
  CHECK(
    frame_parse(header, sizeof(header), &payload) ==
      FRAME_PARSE_PAYLOAD_TOO_LARGE
  );
  return true;
}

static bool test_rejects_bad_crc(void) {
  const uint8_t source[] = {
    UINT8_C(0x01),
    UINT8_C(0x02),
    UINT8_C(0x03),
  };
  uint8_t frame[sizeof(source) + FRAME_OVERHEAD];
  frame_payload_view_t payload;
  const size_t frameLength = buildFrame(
    frame,
    source,
    (uint16_t)sizeof(source)
  );

  frame[5] ^= UINT8_C(0x80);
  CHECK(
    frame_parse(frame, frameLength, &payload) == FRAME_PARSE_BAD_CRC
  );
  buildFrame(frame, source, (uint16_t)sizeof(source));
  frame[frameLength - 1u] ^= UINT8_C(0x01);
  CHECK(
    frame_parse(frame, frameLength, &payload) == FRAME_PARSE_BAD_CRC
  );
  return true;
}

static bool test_null_arguments_and_output_reset(void) {
  uint8_t frame[FRAME_OVERHEAD];
  frame_payload_view_t payload = {
    frame,
    99u,
  };
  const size_t frameLength = buildFrame(frame, NULL, 0u);

  CHECK(
    frame_parse(NULL, frameLength, &payload) ==
      FRAME_PARSE_NULL_ARGUMENT
  );
  CHECK(payload.data == NULL);
  CHECK(payload.length == 0u);
  CHECK(
    frame_parse(frame, frameLength, NULL) ==
      FRAME_PARSE_NULL_ARGUMENT
  );
  frame[0] = 0u;
  payload.data = frame;
  payload.length = 99u;
  CHECK(
    frame_parse(frame, frameLength, &payload) == FRAME_PARSE_BAD_MAGIC
  );
  CHECK(payload.data == NULL);
  CHECK(payload.length == 0u);
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "valid empty payload", test_valid_empty_payload },
    { "valid unaligned maximum payload", test_valid_unaligned_maximum_payload },
    { "every truncation boundary", test_every_truncation_boundary },
    { "trailing data", test_rejects_trailing_data },
    { "bad magic and version", test_rejects_bad_magic_and_version },
    { "oversized payload", test_rejects_oversized_payload },
    { "bad CRC", test_rejects_bad_crc },
    { "null arguments and output reset", test_null_arguments_and_output_reset },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) return 1;
    printf("ok - %s\n", tests[index].name);
  }
  return 0;
}

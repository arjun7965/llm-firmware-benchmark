#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdatomic.h>

#include "ring_buffer.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
              __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

static bool test_rejects_invalid_initialization(void) {
  ring_buffer_t ring;
  uint8_t storage[4];
  CHECK(!ring_buffer_init(NULL, storage, 4u));
  CHECK(!ring_buffer_init(&ring, NULL, 4u));
  CHECK(!ring_buffer_init(&ring, storage, 0u));
  CHECK(!ring_buffer_init(&ring, storage, 3u));
  return true;
}

static bool test_empty_and_fifo_order(void) {
  ring_buffer_t ring;
  uint8_t storage[8];
  uint8_t value = UINT8_C(0xFF);
  CHECK(ring_buffer_init(&ring, storage, 8u));
  CHECK(!ring_buffer_pop(&ring, &value));
  CHECK(value == UINT8_C(0xFF));
  CHECK(ring_buffer_push(&ring, UINT8_C(0x10)));
  CHECK(ring_buffer_push(&ring, UINT8_C(0x20)));
  CHECK(ring_buffer_push(&ring, UINT8_C(0x30)));
  CHECK(ring_buffer_pop(&ring, &value));
  CHECK(value == UINT8_C(0x10));
  CHECK(ring_buffer_pop(&ring, &value));
  CHECK(value == UINT8_C(0x20));
  CHECK(ring_buffer_pop(&ring, &value));
  CHECK(value == UINT8_C(0x30));
  CHECK(!ring_buffer_pop(&ring, &value));
  return true;
}

static bool test_uses_full_capacity_and_drops_newest(void) {
  ring_buffer_t ring;
  uint8_t storage[4];
  uint8_t value;
  CHECK(ring_buffer_init(&ring, storage, 4u));
  for (uint8_t index = 0u; index < 4u; index++) {
    CHECK(ring_buffer_push(&ring, (uint8_t)(index + 1u)));
  }
  CHECK(!ring_buffer_push(&ring, UINT8_C(99)));
  for (uint8_t expected = 1u; expected <= 4u; expected++) {
    CHECK(ring_buffer_pop(&ring, &value));
    CHECK(value == expected);
  }
  CHECK(!ring_buffer_pop(&ring, &value));
  return true;
}

static bool test_capacity_one(void) {
  ring_buffer_t ring;
  uint8_t storage[1];
  uint8_t value;
  CHECK(ring_buffer_init(&ring, storage, 1u));
  CHECK(ring_buffer_push(&ring, UINT8_C(0xA5)));
  CHECK(!ring_buffer_push(&ring, UINT8_C(0x5A)));
  CHECK(ring_buffer_pop(&ring, &value));
  CHECK(value == UINT8_C(0xA5));
  CHECK(!ring_buffer_pop(&ring, &value));
  return true;
}

static bool test_reuses_slots_across_many_wraps(void) {
  ring_buffer_t ring;
  uint8_t storage[8];
  uint8_t value;
  CHECK(ring_buffer_init(&ring, storage, 8u));
  for (size_t cycle = 0u; cycle < 100u; cycle++) {
    for (uint8_t offset = 0u; offset < 8u; offset++) {
      CHECK(ring_buffer_push(
        &ring,
        (uint8_t)(cycle + (size_t)offset)
      ));
    }
    for (uint8_t offset = 0u; offset < 8u; offset++) {
      CHECK(ring_buffer_pop(&ring, &value));
      CHECK(value == (uint8_t)(cycle + (size_t)offset));
    }
  }
  return true;
}

static bool test_unsigned_counter_overflow(void) {
  ring_buffer_t ring;
  uint8_t storage[4];
  uint8_t value;
  CHECK(ring_buffer_init(&ring, storage, 4u));
  atomic_store_explicit(&ring.head, SIZE_MAX - 1u, memory_order_relaxed);
  atomic_store_explicit(&ring.tail, SIZE_MAX - 1u, memory_order_relaxed);

  for (uint8_t expected = 1u; expected <= 4u; expected++) {
    CHECK(ring_buffer_push(&ring, expected));
  }
  CHECK(!ring_buffer_push(&ring, UINT8_C(5)));
  for (uint8_t expected = 1u; expected <= 4u; expected++) {
    CHECK(ring_buffer_pop(&ring, &value));
    CHECK(value == expected);
  }
  CHECK(!ring_buffer_pop(&ring, &value));
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "invalid initialization", test_rejects_invalid_initialization },
    { "empty and FIFO order", test_empty_and_fifo_order },
    { "full capacity and overflow", test_uses_full_capacity_and_drops_newest },
    { "capacity one", test_capacity_one },
    { "slot reuse", test_reuses_slots_across_many_wraps },
    { "counter overflow", test_unsigned_counter_overflow },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) return 1;
    printf("ok - %s\n", tests[index].name);
  }
  return 0;
}

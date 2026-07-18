#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "static_memory_pool.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
              __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

static bool allocation_state_equals(
  const static_memory_pool_t *left,
  const static_memory_pool_t *right
) {
  if (left->initialized != right->initialized) return false;
  for (size_t index = 0u; index < STATIC_MEMORY_POOL_BLOCK_COUNT; index++) {
    if (left->allocated[index] != right->allocated[index]) return false;
  }
  return true;
}

static bool test_initialization_and_invalid_arguments(void) {
  static_memory_pool_t pool = {
    .allocated = { 1u, 1u, 1u, 1u },
    .initialized = false,
  };
  const static_memory_pool_t before = pool;

  CHECK(!static_memory_pool_init(NULL));
  CHECK(allocation_state_equals(&pool, &before));
  CHECK(static_memory_pool_allocate(NULL) == NULL);
  CHECK(static_memory_pool_available(NULL) == 0u);
  CHECK(!static_memory_pool_release(NULL, NULL));
  CHECK(static_memory_pool_allocate(&pool) == NULL);
  CHECK(static_memory_pool_available(&pool) == 0u);
  CHECK(!static_memory_pool_release(&pool, &pool.storage[0u][0u]));

  CHECK(static_memory_pool_init(&pool));
  CHECK(pool.initialized);
  CHECK(static_memory_pool_available(&pool) == STATIC_MEMORY_POOL_BLOCK_COUNT);
  for (size_t index = 0u; index < STATIC_MEMORY_POOL_BLOCK_COUNT; index++) {
    CHECK(pool.allocated[index] == 0u);
  }
  return true;
}

static bool test_static_capacity_alignment_and_exhaustion(void) {
  static static_memory_pool_t pool;
  void *blocks[STATIC_MEMORY_POOL_BLOCK_COUNT] = { 0 };

  CHECK(static_memory_pool_init(&pool));
  for (size_t index = 0u; index < STATIC_MEMORY_POOL_BLOCK_COUNT; index++) {
    blocks[index] = static_memory_pool_allocate(&pool);
    CHECK(blocks[index] != NULL);
    CHECK(
      (uintptr_t)blocks[index] %
        (uintptr_t)STATIC_MEMORY_POOL_ALIGNMENT == 0u
    );
    ((uint8_t *)blocks[index])[0u] = (uint8_t)(index + 1u);
    ((uint8_t *)blocks[index])[STATIC_MEMORY_POOL_BLOCK_SIZE - 1u] =
      (uint8_t)(index + 17u);
    if (index > 0u) {
      CHECK(
        (uintptr_t)blocks[index] - (uintptr_t)blocks[index - 1u] ==
          (uintptr_t)STATIC_MEMORY_POOL_BLOCK_SIZE
      );
    }
  }
  CHECK(static_memory_pool_available(&pool) == 0u);
  CHECK(static_memory_pool_allocate(&pool) == NULL);
  for (size_t index = 0u; index < STATIC_MEMORY_POOL_BLOCK_COUNT; index++) {
    CHECK(((uint8_t *)blocks[index])[0u] == (uint8_t)(index + 1u));
    CHECK(
      ((uint8_t *)blocks[index])[STATIC_MEMORY_POOL_BLOCK_SIZE - 1u] ==
        (uint8_t)(index + 17u)
    );
  }
  return true;
}

static bool test_release_validation_reuse_and_reinitialization(void) {
  static_memory_pool_t pool = { 0 };
  uint8_t outside[STATIC_MEMORY_POOL_BLOCK_SIZE] = { 0 };
  void *first;
  void *second;
  void *third;
  const static_memory_pool_t before_invalid = pool;

  CHECK(!static_memory_pool_release(&pool, outside));
  CHECK(allocation_state_equals(&pool, &before_invalid));
  CHECK(static_memory_pool_init(&pool));
  first = static_memory_pool_allocate(&pool);
  second = static_memory_pool_allocate(&pool);
  third = static_memory_pool_allocate(&pool);
  CHECK(first != NULL && second != NULL && third != NULL);
  CHECK(static_memory_pool_available(&pool) == 1u);

  CHECK(!static_memory_pool_release(&pool, (uint8_t *)second + 1u));
  CHECK(static_memory_pool_available(&pool) == 1u);
  CHECK(!static_memory_pool_release(&pool, outside));
  CHECK(static_memory_pool_available(&pool) == 1u);
  CHECK(static_memory_pool_release(&pool, second));
  CHECK(static_memory_pool_available(&pool) == 2u);
  CHECK(static_memory_pool_allocate(&pool) == second);
  CHECK(static_memory_pool_release(&pool, second));
  CHECK(!static_memory_pool_release(&pool, second));
  CHECK(static_memory_pool_available(&pool) == 2u);

  CHECK(static_memory_pool_init(&pool));
  CHECK(static_memory_pool_available(&pool) == STATIC_MEMORY_POOL_BLOCK_COUNT);
  CHECK(static_memory_pool_allocate(&pool) == first);
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "initialization and invalid arguments", test_initialization_and_invalid_arguments },
    { "static capacity, alignment, and exhaustion", test_static_capacity_alignment_and_exhaustion },
    { "release validation, reuse, and reinitialization", test_release_validation_reuse_and_reinitialization },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) return 1;
    printf("ok - %s\n", tests[index].name);
  }
  return 0;
}

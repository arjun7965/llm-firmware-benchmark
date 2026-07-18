#ifndef STATIC_MEMORY_POOL_H
#define STATIC_MEMORY_POOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdalign.h>

#define STATIC_MEMORY_POOL_BLOCK_COUNT 4u
#define STATIC_MEMORY_POOL_BLOCK_SIZE 32u
#define STATIC_MEMORY_POOL_ALIGNMENT 16u

_Static_assert(
  STATIC_MEMORY_POOL_BLOCK_SIZE % STATIC_MEMORY_POOL_ALIGNMENT == 0u,
  "each block must preserve the pool alignment"
);

typedef struct {
  alignas(STATIC_MEMORY_POOL_ALIGNMENT)
    uint8_t storage[STATIC_MEMORY_POOL_BLOCK_COUNT][STATIC_MEMORY_POOL_BLOCK_SIZE];
  uint8_t allocated[STATIC_MEMORY_POOL_BLOCK_COUNT];
  bool initialized;
} static_memory_pool_t;

bool static_memory_pool_init(static_memory_pool_t *pool);
void *static_memory_pool_allocate(static_memory_pool_t *pool);
bool static_memory_pool_release(static_memory_pool_t *pool, void *block);
size_t static_memory_pool_available(const static_memory_pool_t *pool);

#endif

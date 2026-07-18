#include "static_memory_pool.h"

static bool pool_is_initialized(const static_memory_pool_t *pool) {
  return pool != NULL && pool->initialized;
}

static bool block_to_index(
  const static_memory_pool_t *pool,
  const void *block,
  size_t *index
) {
  if (pool == NULL || block == NULL || index == NULL) return false;

  const uintptr_t base = (uintptr_t)&pool->storage[0u][0u];
  const uintptr_t address = (uintptr_t)block;
  const uintptr_t storage_bytes =
    (uintptr_t)STATIC_MEMORY_POOL_BLOCK_COUNT *
    (uintptr_t)STATIC_MEMORY_POOL_BLOCK_SIZE;
  if (address < base) return false;

  const uintptr_t offset = address - base;
  if (
    offset >= storage_bytes ||
    offset % (uintptr_t)STATIC_MEMORY_POOL_BLOCK_SIZE != 0u
  ) {
    return false;
  }

  *index = (size_t)(offset / (uintptr_t)STATIC_MEMORY_POOL_BLOCK_SIZE);
  return true;
}

bool static_memory_pool_init(static_memory_pool_t *pool) {
  if (pool == NULL) return false;

  for (size_t index = 0u; index < STATIC_MEMORY_POOL_BLOCK_COUNT; index++) {
    pool->allocated[index] = 0u;
  }
  pool->initialized = true;
  return true;
}

void *static_memory_pool_allocate(static_memory_pool_t *pool) {
  if (!pool_is_initialized(pool)) return NULL;

  for (size_t index = 0u; index < STATIC_MEMORY_POOL_BLOCK_COUNT; index++) {
    if (pool->allocated[index] == 0u) {
      pool->allocated[index] = true;
      return pool->storage[index];
    }
  }
  return NULL;
}

bool static_memory_pool_release(static_memory_pool_t *pool, void *block) {
  size_t index;

  if (!pool_is_initialized(pool) || !block_to_index(pool, block, &index)) {
    return false;
  }
  if (!pool->allocated[index]) return false;

  pool->allocated[index] = false;
  return true;
}

size_t static_memory_pool_available(const static_memory_pool_t *pool) {
  if (!pool_is_initialized(pool)) return 0u;

  size_t available = 0u;
  for (size_t index = 0u; index < STATIC_MEMORY_POOL_BLOCK_COUNT; index++) {
    if (pool->allocated[index] == 0u) available++;
  }
  return available;
}

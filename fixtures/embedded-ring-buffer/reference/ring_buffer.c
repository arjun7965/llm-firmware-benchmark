#include "ring_buffer.h"

bool ring_buffer_init(
  ring_buffer_t *ring,
  uint8_t *storage,
  size_t capacity
) {
  if (
    ring == NULL ||
    storage == NULL ||
    capacity == 0u ||
    (capacity & (capacity - 1u)) != 0u
  ) {
    return false;
  }

  ring->storage = storage;
  ring->capacity = capacity;
  atomic_init(&ring->head, 0u);
  atomic_init(&ring->tail, 0u);
  return atomic_is_lock_free(&ring->head) &&
    atomic_is_lock_free(&ring->tail);
}

bool ring_buffer_push(ring_buffer_t *ring, uint8_t value) {
  if (ring == NULL) return false;

  const size_t head = atomic_load_explicit(
    &ring->head,
    memory_order_relaxed
  );
  const size_t tail = atomic_load_explicit(
    &ring->tail,
    memory_order_acquire
  );
  if (head - tail >= ring->capacity) return false;

  ring->storage[head & (ring->capacity - 1u)] = value;
  atomic_store_explicit(
    &ring->head,
    head + 1u,
    memory_order_release
  );
  return true;
}

bool ring_buffer_pop(ring_buffer_t *ring, uint8_t *value) {
  if (ring == NULL || value == NULL) return false;

  const size_t tail = atomic_load_explicit(
    &ring->tail,
    memory_order_relaxed
  );
  const size_t head = atomic_load_explicit(
    &ring->head,
    memory_order_acquire
  );
  if (tail == head) return false;

  *value = ring->storage[tail & (ring->capacity - 1u)];
  atomic_store_explicit(
    &ring->tail,
    tail + 1u,
    memory_order_release
  );
  return true;
}

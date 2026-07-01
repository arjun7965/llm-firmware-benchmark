#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>

typedef struct {
  uint8_t *storage;
  size_t capacity;
  atomic_size_t head;
  atomic_size_t tail;
} ring_buffer_t;

bool ring_buffer_init(
  ring_buffer_t *ring,
  uint8_t *storage,
  size_t capacity
);
bool ring_buffer_push(ring_buffer_t *ring, uint8_t value);
bool ring_buffer_pop(ring_buffer_t *ring, uint8_t *value);

#endif

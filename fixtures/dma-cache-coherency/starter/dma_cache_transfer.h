#ifndef DMA_CACHE_TRANSFER_H
#define DMA_CACHE_TRANSFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_dma_cache.h"

#define DMA_CACHE_LINE_BYTES 32u
#define DMA_BUFFER_ALIGNMENT 4u
#define DMA_MAX_TRANSFER_BYTES 96u

_Static_assert(
  DMA_CACHE_LINE_BYTES % DMA_BUFFER_ALIGNMENT == 0u,
  "cache lines must preserve DMA buffer alignment"
);

typedef struct {
  uint8_t *rx_buffer;
  size_t rx_length;
  bool initialized;
  bool rx_in_flight;
} dma_cache_transfer_t;

bool dma_cache_transfer_init(dma_cache_transfer_t *transfer);
dma_cache_status_t dma_cache_transfer_start_tx(
  dma_cache_transfer_t *transfer,
  const uint8_t *buffer,
  size_t length
);
dma_cache_status_t dma_cache_transfer_start_rx(
  dma_cache_transfer_t *transfer,
  uint8_t *buffer,
  size_t length
);
dma_cache_status_t dma_cache_transfer_finish_rx(dma_cache_transfer_t *transfer);

#endif

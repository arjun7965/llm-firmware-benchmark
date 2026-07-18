#ifndef FIXTURE_DMA_CACHE_H
#define FIXTURE_DMA_CACHE_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
  DMA_CACHE_STATUS_OK = 0,
  DMA_CACHE_STATUS_BUSY,
  DMA_CACHE_STATUS_ERROR,
  DMA_CACHE_STATUS_INVALID_ARGUMENT,
} dma_cache_status_t;

void dma_cache_clean_by_address(const void *address, size_t length);
void dma_cache_invalidate_by_address(void *address, size_t length);
dma_cache_status_t dma_cache_start_tx(const uint8_t *buffer, size_t length);
dma_cache_status_t dma_cache_start_rx(uint8_t *buffer, size_t length);
dma_cache_status_t dma_cache_finish_rx(void);

#endif

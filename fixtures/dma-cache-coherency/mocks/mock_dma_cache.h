#ifndef MOCK_DMA_CACHE_H
#define MOCK_DMA_CACHE_H

#include <stddef.h>

#include "fixture_dma_cache.h"

typedef enum {
  MOCK_DMA_CACHE_OPERATION_CLEAN = 0,
  MOCK_DMA_CACHE_OPERATION_INVALIDATE,
  MOCK_DMA_CACHE_OPERATION_START_TX,
  MOCK_DMA_CACHE_OPERATION_START_RX,
  MOCK_DMA_CACHE_OPERATION_FINISH_RX,
} mock_dma_cache_operation_t;

void mock_dma_cache_reset(void);
void mock_dma_cache_force_next_tx_status(dma_cache_status_t status);
void mock_dma_cache_force_next_rx_start_status(dma_cache_status_t status);
void mock_dma_cache_force_next_rx_finish_status(dma_cache_status_t status);
size_t mock_dma_cache_operation_count(void);
mock_dma_cache_operation_t mock_dma_cache_operation_kind(size_t index);
const void *mock_dma_cache_operation_address(size_t index);
size_t mock_dma_cache_operation_length(size_t index);

#endif

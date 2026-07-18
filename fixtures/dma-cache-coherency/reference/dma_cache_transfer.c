#include "dma_cache_transfer.h"

static bool transfer_is_initialized(const dma_cache_transfer_t *transfer) {
  return transfer != NULL && transfer->initialized;
}

static void clear_rx_state(dma_cache_transfer_t *transfer) {
  transfer->rx_buffer = NULL;
  transfer->rx_length = 0u;
  transfer->rx_in_flight = false;
}

static bool cache_range(
  const void *buffer,
  size_t length,
  uintptr_t *cache_address,
  size_t *cache_length
) {
  const uintptr_t line_mask = (uintptr_t)DMA_CACHE_LINE_BYTES - 1u;
  uintptr_t last_address;
  uintptr_t rounded_end;

  if (
    buffer == NULL ||
    length == 0u ||
    length > DMA_MAX_TRANSFER_BYTES ||
    cache_address == NULL ||
    cache_length == NULL
  ) {
    return false;
  }

  const uintptr_t address = (uintptr_t)buffer;
  if (address % (uintptr_t)DMA_BUFFER_ALIGNMENT != 0u) return false;
  if (address > UINTPTR_MAX - (uintptr_t)(length - 1u)) return false;
  last_address = address + (uintptr_t)(length - 1u);
  if (last_address > UINTPTR_MAX - (uintptr_t)DMA_CACHE_LINE_BYTES) {
    return false;
  }

  *cache_address = address & ~line_mask;
  rounded_end = (last_address + (uintptr_t)DMA_CACHE_LINE_BYTES) & ~line_mask;
  *cache_length = (size_t)(rounded_end - *cache_address);
  return true;
}

bool dma_cache_transfer_init(dma_cache_transfer_t *transfer) {
  if (transfer == NULL) return false;

  clear_rx_state(transfer);
  transfer->initialized = true;
  return true;
}

dma_cache_status_t dma_cache_transfer_start_tx(
  dma_cache_transfer_t *transfer,
  const uint8_t *buffer,
  size_t length
) {
  uintptr_t cache_address;
  size_t cache_length;

  if (
    !transfer_is_initialized(transfer) ||
    !cache_range(buffer, length, &cache_address, &cache_length)
  ) {
    return DMA_CACHE_STATUS_INVALID_ARGUMENT;
  }

  dma_cache_clean_by_address((const void *)cache_address, cache_length);
  return dma_cache_start_tx(buffer, length);
}

dma_cache_status_t dma_cache_transfer_start_rx(
  dma_cache_transfer_t *transfer,
  uint8_t *buffer,
  size_t length
) {
  uintptr_t cache_address;
  size_t cache_length;
  dma_cache_status_t status;

  if (
    !transfer_is_initialized(transfer) ||
    transfer->rx_in_flight ||
    !cache_range(buffer, length, &cache_address, &cache_length)
  ) {
    return DMA_CACHE_STATUS_INVALID_ARGUMENT;
  }

  dma_cache_invalidate_by_address((void *)cache_address, cache_length);
  status = dma_cache_start_rx(buffer, length);
  if (status != DMA_CACHE_STATUS_OK) return status;

  transfer->rx_buffer = buffer;
  transfer->rx_length = length;
  transfer->rx_in_flight = true;
  return DMA_CACHE_STATUS_OK;
}

dma_cache_status_t dma_cache_transfer_finish_rx(dma_cache_transfer_t *transfer) {
  uintptr_t cache_address;
  size_t cache_length;
  dma_cache_status_t status;

  if (!transfer_is_initialized(transfer) || !transfer->rx_in_flight) {
    return DMA_CACHE_STATUS_INVALID_ARGUMENT;
  }

  status = dma_cache_finish_rx();
  if (status == DMA_CACHE_STATUS_BUSY) return status;
  if (status != DMA_CACHE_STATUS_OK) {
    clear_rx_state(transfer);
    return status;
  }
  if (!cache_range(
    transfer->rx_buffer,
    transfer->rx_length,
    &cache_address,
    &cache_length
  )) {
    clear_rx_state(transfer);
    return DMA_CACHE_STATUS_INVALID_ARGUMENT;
  }

  dma_cache_invalidate_by_address((void *)cache_address, cache_length);
  clear_rx_state(transfer);
  return DMA_CACHE_STATUS_OK;
}

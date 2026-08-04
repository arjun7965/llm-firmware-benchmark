#include "compiler_trace_debug.h"

compiler_trace_result_t compiler_trace_transfer_next(
  compiler_trace_transfer_t *transfer,
  compiler_trace_chunk_t *chunk
) {
  uint8_t remaining_bytes;
  uint8_t chunk_bytes;

  if (transfer == NULL || chunk == NULL ||
      !transfer->initialized || transfer->complete) {
    return COMPILER_TRACE_RESULT_INVALID_ARGUMENT;
  }

  remaining_bytes =
    transfer->total_bytes - transfer->next_offset_bytes;
  if (remaining_bytes == 0u) {
    transfer->complete = true;
    return COMPILER_TRACE_RESULT_COMPLETE;
  }
  chunk_bytes = remaining_bytes > COMPILER_TRACE_DMA_MAX_CHUNK_BYTES
    ? COMPILER_TRACE_DMA_MAX_CHUNK_BYTES
    : remaining_bytes;
  chunk->offset_bytes = transfer->next_offset_bytes;
  chunk->length_bytes = chunk_bytes;
  chunk->final_chunk = chunk_bytes == remaining_bytes;
  transfer->next_offset_bytes += chunk_bytes;
  return COMPILER_TRACE_RESULT_CHUNK_READY;
}

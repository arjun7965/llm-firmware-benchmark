#include "compiler_trace_debug.h"

static bool transfer_state_valid(
  const compiler_trace_transfer_t *transfer
) {
  if (!transfer->initialized) return false;
  if (
    transfer->total_bytes == 0u ||
    transfer->total_bytes > COMPILER_TRACE_MAX_TRANSFER_BYTES
  ) return false;
  if (transfer->next_offset_bytes > transfer->total_bytes) return false;
  return transfer->complete ==
    (transfer->next_offset_bytes == transfer->total_bytes);
}

bool compiler_trace_diagnose(compiler_trace_diagnosis_t *diagnosis) {
  if (diagnosis == NULL) return false;

  *diagnosis = (compiler_trace_diagnosis_t){
    .primary_cause = COMPILER_TRACE_CAUSE_UINT8_REMAINING_TRUNCATION,
    .repair = COMPILER_TRACE_REPAIR_KEEP_REMAINING_UINT32,
    .diagnostic_line = UINT32_C(16),
    .transfer_bytes = UINT32_C(320),
    .first_chunk_bytes = UINT32_C(64),
    .divergent_offset_bytes = UINT32_C(64),
    .observed_remaining_bytes = UINT32_C(0),
    .expected_remaining_bytes = UINT32_C(256),
    .source_width_bits = UINT32_C(32),
    .narrowed_width_bits = UINT32_C(8),
    .conversion_warning = true,
    .premature_completion = true,
  };
  return true;
}

bool compiler_trace_transfer_init(
  compiler_trace_transfer_t *transfer,
  uint32_t total_bytes
) {
  if (
    transfer == NULL ||
    total_bytes == 0u ||
    total_bytes > COMPILER_TRACE_MAX_TRANSFER_BYTES
  ) return false;

  *transfer = (compiler_trace_transfer_t){
    .total_bytes = total_bytes,
    .next_offset_bytes = 0u,
    .initialized = true,
    .complete = false,
  };
  return true;
}

compiler_trace_result_t compiler_trace_transfer_next(
  compiler_trace_transfer_t *transfer,
  compiler_trace_chunk_t *chunk
) {
  uint32_t remaining_bytes;
  uint32_t chunk_bytes;
  compiler_trace_chunk_t planned;

  if (
    transfer == NULL ||
    chunk == NULL ||
    !transfer_state_valid(transfer)
  ) return COMPILER_TRACE_RESULT_INVALID_ARGUMENT;
  if (transfer->complete) return COMPILER_TRACE_RESULT_COMPLETE;

  remaining_bytes =
    transfer->total_bytes - transfer->next_offset_bytes;
  chunk_bytes = remaining_bytes > COMPILER_TRACE_DMA_MAX_CHUNK_BYTES
    ? COMPILER_TRACE_DMA_MAX_CHUNK_BYTES
    : remaining_bytes;
  planned = (compiler_trace_chunk_t){
    .offset_bytes = transfer->next_offset_bytes,
    .length_bytes = chunk_bytes,
    .final_chunk = chunk_bytes == remaining_bytes,
  };

  *chunk = planned;
  transfer->next_offset_bytes += chunk_bytes;
  transfer->complete = transfer->next_offset_bytes == transfer->total_bytes;
  return COMPILER_TRACE_RESULT_CHUNK_READY;
}

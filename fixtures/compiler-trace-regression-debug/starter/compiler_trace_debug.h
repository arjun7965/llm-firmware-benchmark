#ifndef COMPILER_TRACE_DEBUG_H
#define COMPILER_TRACE_DEBUG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define COMPILER_TRACE_MAX_TRANSFER_BYTES UINT32_C(512)
#define COMPILER_TRACE_DMA_MAX_CHUNK_BYTES UINT32_C(64)

typedef enum {
  COMPILER_TRACE_CAUSE_UNKNOWN = 0,
  COMPILER_TRACE_CAUSE_UINT8_REMAINING_TRUNCATION,
} compiler_trace_cause_t;

typedef enum {
  COMPILER_TRACE_REPAIR_UNKNOWN = 0,
  COMPILER_TRACE_REPAIR_KEEP_REMAINING_UINT32,
} compiler_trace_repair_t;

typedef struct {
  compiler_trace_cause_t primary_cause;
  compiler_trace_repair_t repair;
  uint32_t diagnostic_line;
  uint32_t transfer_bytes;
  uint32_t first_chunk_bytes;
  uint32_t divergent_offset_bytes;
  uint32_t observed_remaining_bytes;
  uint32_t expected_remaining_bytes;
  uint32_t source_width_bits;
  uint32_t narrowed_width_bits;
  bool conversion_warning;
  bool premature_completion;
} compiler_trace_diagnosis_t;

typedef enum {
  COMPILER_TRACE_RESULT_INVALID_ARGUMENT = 0,
  COMPILER_TRACE_RESULT_CHUNK_READY,
  COMPILER_TRACE_RESULT_COMPLETE,
} compiler_trace_result_t;

typedef struct {
  uint32_t offset_bytes;
  uint32_t length_bytes;
  bool final_chunk;
} compiler_trace_chunk_t;

typedef struct {
  uint32_t total_bytes;
  uint32_t next_offset_bytes;
  bool initialized;
  bool complete;
} compiler_trace_transfer_t;

bool compiler_trace_diagnose(compiler_trace_diagnosis_t *diagnosis);
bool compiler_trace_transfer_init(
  compiler_trace_transfer_t *transfer,
  uint32_t total_bytes
);
compiler_trace_result_t compiler_trace_transfer_next(
  compiler_trace_transfer_t *transfer,
  compiler_trace_chunk_t *chunk
);

#endif

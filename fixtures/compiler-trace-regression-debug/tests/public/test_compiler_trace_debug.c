#include "compiler_trace_debug.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #condition); \
    return false; \
  } \
} while (0)

static bool diagnosis_matches(
  const compiler_trace_diagnosis_t *diagnosis
) {
  return
    diagnosis->primary_cause ==
      COMPILER_TRACE_CAUSE_UINT8_REMAINING_TRUNCATION &&
    diagnosis->repair == COMPILER_TRACE_REPAIR_KEEP_REMAINING_UINT32 &&
    diagnosis->diagnostic_line == UINT32_C(16) &&
    diagnosis->transfer_bytes == UINT32_C(320) &&
    diagnosis->first_chunk_bytes == UINT32_C(64) &&
    diagnosis->divergent_offset_bytes == UINT32_C(64) &&
    diagnosis->observed_remaining_bytes == UINT32_C(0) &&
    diagnosis->expected_remaining_bytes == UINT32_C(256) &&
    diagnosis->source_width_bits == UINT32_C(32) &&
    diagnosis->narrowed_width_bits == UINT32_C(8) &&
    diagnosis->conversion_warning &&
    diagnosis->premature_completion;
}

static bool test_diagnosis_correlates_warning_and_trace(void) {
  compiler_trace_diagnosis_t diagnosis;

  memset(&diagnosis, 0xa5, sizeof(diagnosis));
  CHECK(compiler_trace_diagnose(&diagnosis));
  CHECK(diagnosis_matches(&diagnosis));
  CHECK(
    diagnosis.transfer_bytes - diagnosis.divergent_offset_bytes ==
      diagnosis.expected_remaining_bytes
  );
  CHECK(
    (uint8_t)diagnosis.expected_remaining_bytes ==
      diagnosis.observed_remaining_bytes
  );
  return true;
}

static bool test_null_diagnosis_fails_closed(void) {
  CHECK(!compiler_trace_diagnose(NULL));
  return true;
}

static bool transfer_matches(
  const compiler_trace_transfer_t *transfer,
  uint32_t total_bytes,
  uint32_t next_offset_bytes,
  bool complete
) {
  return transfer->total_bytes == total_bytes &&
    transfer->next_offset_bytes == next_offset_bytes &&
    transfer->initialized &&
    transfer->complete == complete;
}

static bool test_initialization_validates_and_replaces_state(void) {
  compiler_trace_transfer_t transfer;
  compiler_trace_transfer_t before;

  memset(&transfer, 0xa5, sizeof(transfer));
  before = transfer;
  CHECK(!compiler_trace_transfer_init(NULL, UINT32_C(1)));
  CHECK(!compiler_trace_transfer_init(&transfer, UINT32_C(0)));
  CHECK(memcmp(&transfer, &before, sizeof(transfer)) == 0);
  CHECK(
    !compiler_trace_transfer_init(
      &transfer,
      COMPILER_TRACE_MAX_TRANSFER_BYTES + UINT32_C(1)
    )
  );
  CHECK(memcmp(&transfer, &before, sizeof(transfer)) == 0);

  CHECK(compiler_trace_transfer_init(&transfer, UINT32_C(1)));
  CHECK(transfer_matches(&transfer, UINT32_C(1), UINT32_C(0), false));
  CHECK(
    compiler_trace_transfer_init(
      &transfer,
      COMPILER_TRACE_MAX_TRANSFER_BYTES
    )
  );
  CHECK(
    transfer_matches(
      &transfer,
      COMPILER_TRACE_MAX_TRANSFER_BYTES,
      UINT32_C(0),
      false
    )
  );
  return true;
}

static bool expect_chunk(
  compiler_trace_transfer_t *transfer,
  uint32_t expected_offset,
  uint32_t expected_length,
  bool expected_final
) {
  compiler_trace_chunk_t chunk = {
    .offset_bytes = UINT32_C(0xaaaaaaaa),
    .length_bytes = UINT32_C(0xbbbbbbbb),
    .final_chunk = !expected_final,
  };

  CHECK(
    compiler_trace_transfer_next(transfer, &chunk) ==
      COMPILER_TRACE_RESULT_CHUNK_READY
  );
  CHECK(chunk.offset_bytes == expected_offset);
  CHECK(chunk.length_bytes == expected_length);
  CHECK(chunk.final_chunk == expected_final);
  CHECK(
    transfer->next_offset_bytes == expected_offset + expected_length
  );
  CHECK(transfer->complete == expected_final);
  return true;
}

static bool test_repairs_captured_320_byte_trace(void) {
  compiler_trace_transfer_t transfer;
  compiler_trace_transfer_t completed;
  compiler_trace_chunk_t chunk = {
    .offset_bytes = UINT32_C(0x11111111),
    .length_bytes = UINT32_C(0x22222222),
    .final_chunk = false,
  };
  compiler_trace_chunk_t before;
  uint32_t offset;

  CHECK(compiler_trace_transfer_init(&transfer, UINT32_C(320)));
  for (offset = 0u; offset < UINT32_C(320); offset += UINT32_C(64)) {
    CHECK(
      expect_chunk(
        &transfer,
        offset,
        UINT32_C(64),
        offset == UINT32_C(256)
      )
    );
  }
  CHECK(transfer_matches(&transfer, UINT32_C(320), UINT32_C(320), true));

  completed = transfer;
  before = chunk;
  CHECK(
    compiler_trace_transfer_next(&transfer, &chunk) ==
      COMPILER_TRACE_RESULT_COMPLETE
  );
  CHECK(memcmp(&transfer, &completed, sizeof(transfer)) == 0);
  CHECK(memcmp(&chunk, &before, sizeof(chunk)) == 0);
  return true;
}

static bool run_transfer(uint32_t total_bytes) {
  compiler_trace_transfer_t transfer;
  compiler_trace_chunk_t chunk;
  uint32_t expected_offset = 0u;

  CHECK(compiler_trace_transfer_init(&transfer, total_bytes));
  while (expected_offset < total_bytes) {
    const uint32_t remaining = total_bytes - expected_offset;
    const uint32_t expected_length =
      remaining > COMPILER_TRACE_DMA_MAX_CHUNK_BYTES
        ? COMPILER_TRACE_DMA_MAX_CHUNK_BYTES
        : remaining;

    CHECK(
      expect_chunk(
        &transfer,
        expected_offset,
        expected_length,
        expected_length == remaining
      )
    );
    expected_offset += expected_length;
  }
  CHECK(transfer.next_offset_bytes == total_bytes);
  CHECK(transfer.complete);
  CHECK(
    compiler_trace_transfer_next(&transfer, &chunk) ==
      COMPILER_TRACE_RESULT_COMPLETE
  );
  return true;
}

static bool test_chunk_boundaries_and_wide_remaining_values(void) {
  const uint32_t totals[] = {
    UINT32_C(1),
    UINT32_C(63),
    UINT32_C(64),
    UINT32_C(65),
    UINT32_C(255),
    UINT32_C(256),
    UINT32_C(257),
    COMPILER_TRACE_MAX_TRANSFER_BYTES,
  };
  size_t index;

  for (index = 0u; index < sizeof(totals) / sizeof(totals[0]); ++index) {
    CHECK(run_transfer(totals[index]));
  }
  return true;
}

static bool invalid_next_has_no_effect(
  compiler_trace_transfer_t transfer
) {
  compiler_trace_transfer_t before = transfer;
  compiler_trace_chunk_t chunk = {
    .offset_bytes = UINT32_C(0x12345678),
    .length_bytes = UINT32_C(0x9abcdef0),
    .final_chunk = true,
  };
  compiler_trace_chunk_t chunk_before = chunk;

  CHECK(
    compiler_trace_transfer_next(&transfer, &chunk) ==
      COMPILER_TRACE_RESULT_INVALID_ARGUMENT
  );
  CHECK(memcmp(&transfer, &before, sizeof(transfer)) == 0);
  CHECK(memcmp(&chunk, &chunk_before, sizeof(chunk)) == 0);
  return true;
}

static bool test_next_rejects_invalid_inputs_without_mutation(void) {
  compiler_trace_transfer_t transfer;
  compiler_trace_transfer_t before;
  compiler_trace_chunk_t chunk = {
    .offset_bytes = UINT32_C(0x12345678),
    .length_bytes = UINT32_C(0x9abcdef0),
    .final_chunk = true,
  };
  compiler_trace_chunk_t chunk_before = chunk;

  CHECK(
    compiler_trace_transfer_next(NULL, &chunk) ==
      COMPILER_TRACE_RESULT_INVALID_ARGUMENT
  );
  CHECK(memcmp(&chunk, &chunk_before, sizeof(chunk)) == 0);

  CHECK(compiler_trace_transfer_init(&transfer, UINT32_C(64)));
  before = transfer;
  CHECK(
    compiler_trace_transfer_next(&transfer, NULL) ==
      COMPILER_TRACE_RESULT_INVALID_ARGUMENT
  );
  CHECK(memcmp(&transfer, &before, sizeof(transfer)) == 0);

  CHECK(invalid_next_has_no_effect((compiler_trace_transfer_t){0}));
  CHECK(invalid_next_has_no_effect((compiler_trace_transfer_t){
    .total_bytes = UINT32_C(64),
    .next_offset_bytes = 0u,
    .initialized = false,
    .complete = false,
  }));
  CHECK(invalid_next_has_no_effect((compiler_trace_transfer_t){
    .total_bytes = 0u,
    .next_offset_bytes = 0u,
    .initialized = true,
    .complete = true,
  }));
  CHECK(invalid_next_has_no_effect((compiler_trace_transfer_t){
    .total_bytes = COMPILER_TRACE_MAX_TRANSFER_BYTES + UINT32_C(1),
    .next_offset_bytes = 0u,
    .initialized = true,
    .complete = false,
  }));
  CHECK(invalid_next_has_no_effect((compiler_trace_transfer_t){
    .total_bytes = UINT32_C(64),
    .next_offset_bytes = UINT32_C(65),
    .initialized = true,
    .complete = false,
  }));
  CHECK(invalid_next_has_no_effect((compiler_trace_transfer_t){
    .total_bytes = UINT32_C(64),
    .next_offset_bytes = 0u,
    .initialized = true,
    .complete = true,
  }));
  CHECK(invalid_next_has_no_effect((compiler_trace_transfer_t){
    .total_bytes = UINT32_C(64),
    .next_offset_bytes = UINT32_C(64),
    .initialized = true,
    .complete = false,
  }));
  return true;
}

typedef bool (*test_function_t)(void);

typedef struct {
  const char *name;
  test_function_t function;
} test_case_t;

int main(void) {
  const test_case_t tests[] = {
    {
      "diagnosis correlates warning and trace",
      test_diagnosis_correlates_warning_and_trace,
    },
    {"null diagnosis fails closed", test_null_diagnosis_fails_closed},
    {
      "initialization validates and replaces state",
      test_initialization_validates_and_replaces_state,
    },
    {
      "repair captured 320-byte trace",
      test_repairs_captured_320_byte_trace,
    },
    {
      "chunk boundaries and wide remaining values",
      test_chunk_boundaries_and_wide_remaining_values,
    },
    {
      "next rejects invalid inputs without mutation",
      test_next_rejects_invalid_inputs_without_mutation,
    },
  };
  size_t index;

  for (index = 0u; index < sizeof(tests) / sizeof(tests[0]); ++index) {
    if (!tests[index].function()) return 1;
    printf("ok - %s\n", tests[index].name);
  }
  return 0;
}

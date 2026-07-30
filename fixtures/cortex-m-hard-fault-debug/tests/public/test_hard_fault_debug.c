#include "hard_fault_debug.h"

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

static bool diagnosis_matches(const hard_fault_diagnosis_t *diagnosis) {
  return
    diagnosis->primary_cause ==
      HARD_FAULT_CAUSE_PRECISE_DATA_BUS_WRITE &&
    diagnosis->instruction ==
      HARD_FAULT_INSTRUCTION_INDEXED_WORD_STORE &&
    diagnosis->repair ==
      HARD_FAULT_REPAIR_REJECT_INDEX_AT_CAPACITY &&
    diagnosis->stack_pointer == UINT32_C(0x20000fc0) &&
    diagnosis->stacked_lr == UINT32_C(0x080006e9) &&
    diagnosis->fault_pc == UINT32_C(0x08000536) &&
    diagnosis->fault_address == UINT32_C(0x20002000) &&
    diagnosis->function_start == UINT32_C(0x08000530) &&
    diagnosis->function_offset == UINT32_C(0x00000006) &&
    diagnosis->caller_return_address == UINT32_C(0x080006e8) &&
    diagnosis->caller_function_start == UINT32_C(0x080006d0) &&
    diagnosis->caller_offset == UINT32_C(0x00000018) &&
    diagnosis->object_start == UINT32_C(0x20001fe0) &&
    diagnosis->object_end == UINT32_C(0x20002000) &&
    diagnosis->access_width_bytes == UINT32_C(4) &&
    diagnosis->offending_index == UINT32_C(8) &&
    diagnosis->capacity == HARD_FAULT_LOG_WORDS &&
    diagnosis->stored_value == UINT32_C(0xdeadbeef) &&
    diagnosis->forced_escalation &&
    !diagnosis->bus_fault_enabled &&
    diagnosis->fault_address_valid &&
    diagnosis->stacked_pc_precise &&
    diagnosis->return_to_thread &&
    diagnosis->used_psp &&
    diagnosis->basic_frame;
}

static bool test_null_inputs_fail_closed(void) {
  CHECK(!hard_fault_diagnose(NULL));
  CHECK(!hard_fault_log_store(NULL, UINT32_C(1), 0u));
  return true;
}

static bool test_decodes_fault_and_exception_state(void) {
  hard_fault_diagnosis_t diagnosis = {0};

  CHECK(hard_fault_diagnose(&diagnosis));
  CHECK(
    diagnosis.primary_cause ==
      HARD_FAULT_CAUSE_PRECISE_DATA_BUS_WRITE
  );
  CHECK(diagnosis.forced_escalation);
  CHECK(!diagnosis.bus_fault_enabled);
  CHECK(diagnosis.fault_address_valid);
  CHECK(diagnosis.stacked_pc_precise);
  CHECK(diagnosis.return_to_thread);
  CHECK(diagnosis.used_psp);
  CHECK(diagnosis.basic_frame);
  CHECK(diagnosis.stack_pointer == UINT32_C(0x20000fc0));
  return true;
}

static bool test_symbolizes_fault_and_caller(void) {
  hard_fault_diagnosis_t diagnosis = {0};

  CHECK(hard_fault_diagnose(&diagnosis));
  CHECK(diagnosis.stacked_lr == UINT32_C(0x080006e9));
  CHECK(diagnosis.fault_pc == UINT32_C(0x08000536));
  CHECK(diagnosis.function_start == UINT32_C(0x08000530));
  CHECK(
    diagnosis.function_start + diagnosis.function_offset ==
      diagnosis.fault_pc
  );
  CHECK(diagnosis.caller_return_address == UINT32_C(0x080006e8));
  CHECK((diagnosis.caller_return_address & UINT32_C(1)) == 0u);
  CHECK(diagnosis.caller_function_start == UINT32_C(0x080006d0));
  CHECK(
    diagnosis.caller_function_start + diagnosis.caller_offset ==
      diagnosis.caller_return_address
  );
  return true;
}

static bool test_correlates_store_and_object_boundary(void) {
  hard_fault_diagnosis_t diagnosis = {0};

  CHECK(hard_fault_diagnose(&diagnosis));
  CHECK(
    diagnosis.instruction ==
      HARD_FAULT_INSTRUCTION_INDEXED_WORD_STORE
  );
  CHECK(diagnosis.object_start == UINT32_C(0x20001fe0));
  CHECK(diagnosis.object_end == UINT32_C(0x20002000));
  CHECK(
    diagnosis.object_end - diagnosis.object_start ==
      diagnosis.capacity * diagnosis.access_width_bytes
  );
  CHECK(
    diagnosis.object_start +
      diagnosis.offending_index * diagnosis.access_width_bytes ==
      diagnosis.fault_address
  );
  CHECK(diagnosis.fault_address == diagnosis.object_end);
  CHECK(diagnosis.stored_value == UINT32_C(0xdeadbeef));
  CHECK(
    diagnosis.repair ==
      HARD_FAULT_REPAIR_REJECT_INDEX_AT_CAPACITY
  );
  return true;
}

static bool test_diagnosis_overwrites_stale_output(void) {
  hard_fault_diagnosis_t diagnosis;

  memset(&diagnosis, 0xa5, sizeof(diagnosis));
  CHECK(hard_fault_diagnose(&diagnosis));
  CHECK(diagnosis_matches(&diagnosis));
  return true;
}

typedef struct {
  uint32_t before;
  hard_fault_log_t log;
  uint32_t after;
} guarded_log_t;

static void initialize_guarded_log(guarded_log_t *guarded) {
  size_t index;

  guarded->before = UINT32_C(0x11223344);
  guarded->after = UINT32_C(0x55667788);
  for (index = 0u; index < HARD_FAULT_LOG_WORDS; ++index) {
    guarded->log.words[index] = UINT32_C(0xa0a0a0a0) + (uint32_t)index;
  }
}

static bool guards_match(const guarded_log_t *guarded) {
  return guarded->before == UINT32_C(0x11223344) &&
    guarded->after == UINT32_C(0x55667788);
}

static bool test_repair_accepts_valid_boundary_indices(void) {
  guarded_log_t guarded;
  uint32_t expected[HARD_FAULT_LOG_WORDS];

  initialize_guarded_log(&guarded);
  memcpy(expected, guarded.log.words, sizeof(expected));
  expected[0] = UINT32_C(0x01020304);
  CHECK(
    hard_fault_log_store(&guarded.log, UINT32_C(0x01020304), 0u)
  );
  CHECK(memcmp(expected, guarded.log.words, sizeof(expected)) == 0);
  CHECK(guards_match(&guarded));

  expected[HARD_FAULT_LOG_WORDS - 1u] = UINT32_C(0xf1f2f3f4);
  CHECK(
    hard_fault_log_store(
      &guarded.log,
      UINT32_C(0xf1f2f3f4),
      HARD_FAULT_LOG_WORDS - 1u
    )
  );
  CHECK(memcmp(expected, guarded.log.words, sizeof(expected)) == 0);
  CHECK(guards_match(&guarded));
  return true;
}

static bool test_repair_rejects_one_past_and_large_indices(void) {
  guarded_log_t guarded;
  guarded_log_t before;

  initialize_guarded_log(&guarded);
  before = guarded;
  CHECK(
    !hard_fault_log_store(
      &guarded.log,
      UINT32_C(0xdeadbeef),
      HARD_FAULT_LOG_WORDS
    )
  );
  CHECK(memcmp(&guarded, &before, sizeof(guarded)) == 0);
  CHECK(
    !hard_fault_log_store(
      &guarded.log,
      UINT32_C(0xdeadbeef),
      SIZE_MAX
    )
  );
  CHECK(memcmp(&guarded, &before, sizeof(guarded)) == 0);
  return true;
}

typedef bool (*test_function_t)(void);

typedef struct {
  const char *name;
  test_function_t function;
} test_case_t;

int main(void) {
  const test_case_t tests[] = {
    {"null inputs fail closed", test_null_inputs_fail_closed},
    {
      "decode fault and exception state",
      test_decodes_fault_and_exception_state,
    },
    {
      "symbolize fault and caller",
      test_symbolizes_fault_and_caller,
    },
    {
      "correlate store and object boundary",
      test_correlates_store_and_object_boundary,
    },
    {
      "diagnosis overwrites stale output",
      test_diagnosis_overwrites_stale_output,
    },
    {
      "repair accepts valid boundary indices",
      test_repair_accepts_valid_boundary_indices,
    },
    {
      "repair rejects one-past and large indices",
      test_repair_rejects_one_past_and_large_indices,
    },
  };
  size_t index;

  for (index = 0u; index < sizeof(tests) / sizeof(tests[0]); ++index) {
    if (!tests[index].function()) return 1;
    printf("ok - %s\n", tests[index].name);
  }
  return 0;
}

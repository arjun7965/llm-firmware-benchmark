#include "hard_fault_debug.h"

bool hard_fault_diagnose(hard_fault_diagnosis_t *diagnosis) {
  if (diagnosis == NULL) return false;

  *diagnosis = (hard_fault_diagnosis_t){
    .primary_cause = HARD_FAULT_CAUSE_PRECISE_DATA_BUS_WRITE,
    .instruction = HARD_FAULT_INSTRUCTION_INDEXED_WORD_STORE,
    .repair = HARD_FAULT_REPAIR_REJECT_INDEX_AT_CAPACITY,
    .stack_pointer = UINT32_C(0x20000fc0),
    .stacked_lr = UINT32_C(0x080006e9),
    .fault_pc = UINT32_C(0x08000536),
    .fault_address = UINT32_C(0x20002000),
    .function_start = UINT32_C(0x08000530),
    .function_offset = UINT32_C(0x00000006),
    .caller_return_address = UINT32_C(0x080006e8),
    .caller_function_start = UINT32_C(0x080006d0),
    .caller_offset = UINT32_C(0x00000018),
    .object_start = UINT32_C(0x20001fe0),
    .object_end = UINT32_C(0x20002000),
    .access_width_bytes = UINT32_C(4),
    .offending_index = UINT32_C(8),
    .capacity = HARD_FAULT_LOG_WORDS,
    .stored_value = UINT32_C(0xdeadbeef),
    .forced_escalation = true,
    .bus_fault_enabled = false,
    .fault_address_valid = true,
    .stacked_pc_precise = true,
    .return_to_thread = true,
    .used_psp = true,
    .basic_frame = true,
  };
  return true;
}

bool hard_fault_log_store(
  hard_fault_log_t *log,
  uint32_t value,
  size_t index
) {
  if (log == NULL || index >= HARD_FAULT_LOG_WORDS) return false;
  log->words[index] = value;
  return true;
}

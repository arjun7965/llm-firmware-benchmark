#ifndef HARD_FAULT_DEBUG_H
#define HARD_FAULT_DEBUG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HARD_FAULT_LOG_WORDS 8u

typedef enum {
  HARD_FAULT_CAUSE_UNKNOWN = 0,
  HARD_FAULT_CAUSE_PRECISE_DATA_BUS_WRITE,
} hard_fault_cause_t;

typedef enum {
  HARD_FAULT_INSTRUCTION_UNKNOWN = 0,
  HARD_FAULT_INSTRUCTION_INDEXED_WORD_STORE,
} hard_fault_instruction_t;

typedef enum {
  HARD_FAULT_REPAIR_UNKNOWN = 0,
  HARD_FAULT_REPAIR_REJECT_INDEX_AT_CAPACITY,
} hard_fault_repair_t;

typedef struct {
  hard_fault_cause_t primary_cause;
  hard_fault_instruction_t instruction;
  hard_fault_repair_t repair;
  uint32_t stack_pointer;
  uint32_t stacked_lr;
  uint32_t fault_pc;
  uint32_t fault_address;
  uint32_t function_start;
  uint32_t function_offset;
  uint32_t caller_return_address;
  uint32_t caller_function_start;
  uint32_t caller_offset;
  uint32_t object_start;
  uint32_t object_end;
  uint32_t access_width_bytes;
  uint32_t offending_index;
  uint32_t capacity;
  uint32_t stored_value;
  bool forced_escalation;
  bool bus_fault_enabled;
  bool fault_address_valid;
  bool stacked_pc_precise;
  bool return_to_thread;
  bool used_psp;
  bool basic_frame;
} hard_fault_diagnosis_t;

typedef struct {
  uint32_t words[HARD_FAULT_LOG_WORDS];
} hard_fault_log_t;

bool hard_fault_diagnose(hard_fault_diagnosis_t *diagnosis);
bool hard_fault_log_store(
  hard_fault_log_t *log,
  uint32_t value,
  size_t index
);

#endif

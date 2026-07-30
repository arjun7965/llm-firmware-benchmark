#include "hard_fault_debug.h"

bool hard_fault_log_store(
  hard_fault_log_t *log,
  uint32_t value,
  size_t index
) {
  if (log == NULL || index > HARD_FAULT_LOG_WORDS) return false;
  log->words[index] = value;
  return true;
}

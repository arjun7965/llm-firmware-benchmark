#include "fixed_point_stack.h"

static bool stack_configuration_is_valid(
  const uint8_t *stack_base,
  size_t stack_size,
  int16_t gain_q15
) {
  if (
    stack_base == NULL ||
    stack_size < FIXED_POINT_STACK_BUDGET_BYTES ||
    stack_size % FIXED_POINT_STACK_ALIGNMENT != 0u ||
    gain_q15 < 0
  ) {
    return false;
  }
  return (uintptr_t)stack_base % (uintptr_t)FIXED_POINT_STACK_ALIGNMENT == 0u;
}

static bool worker_is_initialized(const fixed_point_worker_t *worker) {
  return worker != NULL && worker->initialized &&
    stack_configuration_is_valid(
      worker->stack_base,
      worker->stack_size,
      worker->gain_q15
    );
}

static int64_t divide_q15_rounded(int64_t value) {
  const int64_t half = (int64_t)FIXED_POINT_Q15_SCALE / 2;

  if (value >= 0) return (value + half) / (int64_t)FIXED_POINT_Q15_SCALE;
  return (value - half) / (int64_t)FIXED_POINT_Q15_SCALE;
}

static int16_t saturate_int16(int64_t value) {
  if (value > INT16_MAX) return INT16_MAX;
  if (value < INT16_MIN) return INT16_MIN;
  return (int16_t)value;
}

bool fixed_point_worker_init(
  fixed_point_worker_t *worker,
  uint8_t *stack_base,
  size_t stack_size,
  int16_t gain_q15,
  int16_t offset_q8_8
) {
  if (
    worker == NULL ||
    !stack_configuration_is_valid(stack_base, stack_size, gain_q15)
  ) {
    return false;
  }

  worker->stack_base = stack_base;
  worker->stack_size = stack_size;
  worker->gain_q15 = gain_q15;
  worker->offset_q8_8 = offset_q8_8;
  worker->initialized = true;
  return true;
}

size_t fixed_point_worker_stack_used(const fixed_point_worker_t *worker) {
  size_t unused = 0u;

  if (!worker_is_initialized(worker)) return 0u;
  while (
    unused < worker->stack_size &&
    worker->stack_base[unused] == FIXED_POINT_STACK_FILL_BYTE
  ) {
    unused++;
  }
  return worker->stack_size - unused;
}

bool fixed_point_worker_process(
  fixed_point_worker_t *worker,
  const int16_t *input_q8_8,
  int16_t *output_q8_8,
  size_t sample_count
) {
  if (
    !worker_is_initialized(worker) ||
    sample_count > FIXED_POINT_MAX_BATCH_SAMPLES ||
    fixed_point_worker_stack_used(worker) > FIXED_POINT_STACK_BUDGET_BYTES ||
    (sample_count > 0u && (input_q8_8 == NULL || output_q8_8 == NULL))
  ) {
    return false;
  }

  for (size_t index = 0u; index < sample_count; index++) {
    const int64_t product =
      (int64_t)input_q8_8[index] * (int64_t)worker->gain_q15;
    const int64_t scaled = divide_q15_rounded(product);
    const int64_t adjusted = scaled + (int64_t)worker->offset_q8_8;

    output_q8_8[index] = saturate_int16(adjusted);
  }
  return true;
}

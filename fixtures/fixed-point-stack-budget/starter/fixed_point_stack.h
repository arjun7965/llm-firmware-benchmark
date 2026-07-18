#ifndef FIXED_POINT_STACK_H
#define FIXED_POINT_STACK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FIXED_POINT_Q15_SCALE 32768u
#define FIXED_POINT_STACK_ALIGNMENT 8u
#define FIXED_POINT_STACK_FILL_BYTE UINT8_C(0xa5)
#define FIXED_POINT_STACK_BUDGET_BYTES 64u
#define FIXED_POINT_MAX_BATCH_SAMPLES 8u

typedef struct {
  uint8_t *stack_base;
  size_t stack_size;
  int16_t gain_q15;
  int16_t offset_q8_8;
  bool initialized;
} fixed_point_worker_t;

bool fixed_point_worker_init(
  fixed_point_worker_t *worker,
  uint8_t *stack_base,
  size_t stack_size,
  int16_t gain_q15,
  int16_t offset_q8_8
);
bool fixed_point_worker_process(
  fixed_point_worker_t *worker,
  const int16_t *input_q8_8,
  int16_t *output_q8_8,
  size_t sample_count
);
size_t fixed_point_worker_stack_used(const fixed_point_worker_t *worker);

#endif

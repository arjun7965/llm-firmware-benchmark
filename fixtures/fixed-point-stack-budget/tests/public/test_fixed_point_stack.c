#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdalign.h>

#include "fixed_point_stack.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
              __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

static void fill_stack(uint8_t *stack, size_t size) {
  for (size_t index = 0u; index < size; index++) {
    stack[index] = FIXED_POINT_STACK_FILL_BYTE;
  }
}

static bool worker_state_equals(
  const fixed_point_worker_t *left,
  const fixed_point_worker_t *right
) {
  return left->stack_base == right->stack_base &&
    left->stack_size == right->stack_size &&
    left->gain_q15 == right->gain_q15 &&
    left->offset_q8_8 == right->offset_q8_8 &&
    left->initialized == right->initialized;
}

static bool test_initialization_contract_and_invalid_work(void) {
  alignas(FIXED_POINT_STACK_ALIGNMENT) uint8_t stack[128u];
  fixed_point_worker_t worker = {
    .stack_base = stack,
    .stack_size = 8u,
    .gain_q15 = 1,
    .offset_q8_8 = 2,
    .initialized = false,
  };
  fixed_point_worker_t uninitialized = { 0 };
  const fixed_point_worker_t before = worker;
  int16_t input = 1;
  int16_t output = 99;

  fill_stack(stack, sizeof(stack));
  CHECK(!fixed_point_worker_init(NULL, stack, sizeof(stack), 1, 0));
  CHECK(worker_state_equals(&worker, &before));
  CHECK(!fixed_point_worker_init(&worker, NULL, sizeof(stack), 1, 0));
  CHECK(worker_state_equals(&worker, &before));
  CHECK(!fixed_point_worker_init(&worker, stack + 1u, sizeof(stack), 1, 0));
  CHECK(worker_state_equals(&worker, &before));
  CHECK(!fixed_point_worker_init(&worker, stack, 63u, 1, 0));
  CHECK(worker_state_equals(&worker, &before));
  CHECK(!fixed_point_worker_init(&worker, stack, sizeof(stack), -1, 0));
  CHECK(worker_state_equals(&worker, &before));
  CHECK(fixed_point_worker_stack_used(&uninitialized) == 0u);
  CHECK(!fixed_point_worker_process(&uninitialized, &input, &output, 1u));
  CHECK(output == 99);

  CHECK(fixed_point_worker_init(&worker, stack, sizeof(stack), 1, 2));
  CHECK(worker.initialized);
  CHECK(worker.stack_base == stack);
  CHECK(worker.stack_size == sizeof(stack));
  CHECK(worker.gain_q15 == 1);
  CHECK(worker.offset_q8_8 == 2);
  return true;
}

static bool test_q15_rounding_offset_and_in_place_batch(void) {
  alignas(FIXED_POINT_STACK_ALIGNMENT) uint8_t stack[128u];
  fixed_point_worker_t worker = { 0 };
  int16_t samples[] = { 256, -256, 1, -1 };

  fill_stack(stack, sizeof(stack));
  CHECK(fixed_point_worker_init(&worker, stack, sizeof(stack), 16384, 64));
  CHECK(fixed_point_worker_stack_used(&worker) == 0u);
  CHECK(fixed_point_worker_process(
    &worker,
    samples,
    samples,
    sizeof(samples) / sizeof(samples[0])
  ));
  CHECK(samples[0] == 192);
  CHECK(samples[1] == -64);
  CHECK(samples[2] == 65);
  CHECK(samples[3] == 63);
  CHECK(fixed_point_worker_process(&worker, NULL, NULL, 0u));
  return true;
}

static bool test_saturating_fixed_point_arithmetic(void) {
  alignas(FIXED_POINT_STACK_ALIGNMENT) uint8_t stack[128u];
  fixed_point_worker_t worker = { 0 };
  int16_t input;
  int16_t output = 0;

  fill_stack(stack, sizeof(stack));
  CHECK(fixed_point_worker_init(
    &worker,
    stack,
    sizeof(stack),
    INT16_MAX,
    INT16_MAX
  ));
  input = INT16_MAX;
  CHECK(fixed_point_worker_process(&worker, &input, &output, 1u));
  CHECK(output == INT16_MAX);

  CHECK(fixed_point_worker_init(
    &worker,
    stack,
    sizeof(stack),
    INT16_MAX,
    INT16_MIN
  ));
  input = INT16_MIN;
  CHECK(fixed_point_worker_process(&worker, &input, &output, 1u));
  CHECK(output == INT16_MIN);
  return true;
}

static bool test_stack_watermark_budget_and_batch_bound(void) {
  alignas(FIXED_POINT_STACK_ALIGNMENT) uint8_t stack[128u];
  fixed_point_worker_t worker = { 0 };
  int16_t input = 256;
  int16_t output = -123;
  int16_t too_many[FIXED_POINT_MAX_BATCH_SAMPLES + 1u] = { 0 };
  int16_t too_many_output[FIXED_POINT_MAX_BATCH_SAMPLES + 1u];

  fill_stack(stack, sizeof(stack));
  for (size_t index = 0u;
    index < sizeof(too_many_output) / sizeof(too_many_output[0]);
    index++) {
    too_many_output[index] = 77;
  }
  CHECK(fixed_point_worker_init(&worker, stack, sizeof(stack), 16384, 0));
  CHECK(fixed_point_worker_stack_used(&worker) == 0u);
  stack[sizeof(stack) - 1u] = 0u;
  CHECK(fixed_point_worker_stack_used(&worker) == 1u);
  CHECK(fixed_point_worker_process(&worker, &input, &output, 1u));
  CHECK(output == 128);

  stack[64u] = 0u;
  CHECK(fixed_point_worker_stack_used(&worker) == FIXED_POINT_STACK_BUDGET_BYTES);
  CHECK(fixed_point_worker_process(&worker, &input, &output, 1u));
  stack[63u] = 0u;
  CHECK(
    fixed_point_worker_stack_used(&worker) == FIXED_POINT_STACK_BUDGET_BYTES + 1u
  );
  output = -123;
  CHECK(!fixed_point_worker_process(&worker, &input, &output, 1u));
  CHECK(output == -123);
  CHECK(!fixed_point_worker_process(
    &worker,
    too_many,
    too_many_output,
    FIXED_POINT_MAX_BATCH_SAMPLES + 1u
  ));
  for (size_t index = 0u;
    index < sizeof(too_many_output) / sizeof(too_many_output[0]);
    index++) {
    CHECK(too_many_output[index] == 77);
  }
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "initialization contract and invalid work", test_initialization_contract_and_invalid_work },
    { "q15 rounding, offset, and in-place batch", test_q15_rounding_offset_and_in_place_batch },
    { "saturating fixed-point arithmetic", test_saturating_fixed_point_arithmetic },
    { "stack watermark budget and batch bound", test_stack_watermark_budget_and_batch_bound },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) return 1;
    printf("ok - %s\n", tests[index].name);
  }
  return 0;
}

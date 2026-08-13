#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "fixed_point_filter.h"
#include "mock_filter_cost.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
              __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

static bool filter_state_equals(
  const fixed_point_filter_t *left,
  const fixed_point_filter_t *right
) {
  if (left->initialized != right->initialized) return false;
  for (size_t index = 0u;
    index < FIXED_POINT_FILTER_HISTORY_SAMPLES;
    index++) {
    if (left->history[index] != right->history[index]) return false;
  }
  return true;
}

typedef struct {
  const fixed_point_filter_t *filter;
  fixed_point_filter_t expected_filter;
  const int16_t *output;
  int16_t expected_output;
} commit_context_t;

static bool state_is_unpublished(const void *value) {
  const commit_context_t *context = value;

  return filter_state_equals(context->filter, &context->expected_filter) &&
    *context->output == context->expected_output;
}

static int64_t divide_q15_rounded(int64_t value) {
  const int64_t half = (int64_t)FIXED_POINT_FILTER_Q15_SCALE / 2;

  return value >= 0
    ? (value + half) / (int64_t)FIXED_POINT_FILTER_Q15_SCALE
    : (value - half) / (int64_t)FIXED_POINT_FILTER_Q15_SCALE;
}

static int16_t saturate_int16(int64_t value) {
  if (value > INT16_MAX) return INT16_MAX;
  if (value < INT16_MIN) return INT16_MIN;
  return (int16_t)value;
}

static int16_t ideal_step(
  const int16_t history[FIXED_POINT_FILTER_HISTORY_SAMPLES],
  int16_t input
) {
  const int64_t accumulator =
    (int64_t)input * FIXED_POINT_FILTER_COEFF_0_Q15 +
    (int64_t)history[0] * FIXED_POINT_FILTER_COEFF_1_Q15 +
    (int64_t)history[1] * FIXED_POINT_FILTER_COEFF_2_Q15 +
    (int64_t)history[2] * FIXED_POINT_FILTER_COEFF_3_Q15 +
    (int64_t)history[3] * FIXED_POINT_FILTER_COEFF_2_Q15 +
    (int64_t)history[4] * FIXED_POINT_FILTER_COEFF_1_Q15 +
    (int64_t)history[5] * FIXED_POINT_FILTER_COEFF_0_Q15;

  return saturate_int16(divide_q15_rounded(accumulator));
}

static void shift_history(
  int16_t history[FIXED_POINT_FILTER_HISTORY_SAMPLES],
  int16_t input
) {
  for (size_t index = FIXED_POINT_FILTER_HISTORY_SAMPLES - 1u;
    index > 0u;
    index--) {
    history[index] = history[index - 1u];
  }
  history[0] = input;
}

static bool test_initialization_and_invalid_calls(void) {
  fixed_point_filter_t filter = {
    .history = { 1, 2, 3, 4, 5, 6 },
    .initialized = false,
  };
  fixed_point_filter_t uninitialized = { 0 };
  const fixed_point_filter_t before = filter;
  const fixed_point_filter_t uninitialized_before = uninitialized;
  fixed_point_filter_t initialized_before;
  int16_t output = 77;

  mock_filter_cost_reset(FIXED_POINT_FILTER_CYCLE_BUDGET);
  CHECK(!fixed_point_filter_init(NULL));
  CHECK(filter_state_equals(&filter, &before));
  CHECK(fixed_point_filter_init(&filter));
  CHECK(filter.initialized);
  for (size_t index = 0u;
    index < FIXED_POINT_FILTER_HISTORY_SAMPLES;
    index++) {
    CHECK(filter.history[index] == 0);
  }
  initialized_before = filter;

  CHECK(!fixed_point_filter_step(
    NULL,
    mock_filter_cost_handle(),
    1,
    &output
  ));
  CHECK(!fixed_point_filter_step(
    &uninitialized,
    mock_filter_cost_handle(),
    1,
    &output
  ));
  CHECK(!fixed_point_filter_step(&filter, NULL, 1, &output));
  CHECK(!fixed_point_filter_step(
    &filter,
    mock_filter_cost_handle(),
    1,
    NULL
  ));
  CHECK(filter_state_equals(&uninitialized, &uninitialized_before));
  CHECK(filter_state_equals(&filter, &initialized_before));
  CHECK(output == 77);
  CHECK(mock_filter_cost_begin_count() == 0u);
  CHECK(mock_filter_cost_mac_count() == 0u);
  CHECK(mock_filter_cost_commit_count() == 0u);
  CHECK(!mock_filter_cost_invalid_access());
  return true;
}

static bool test_cycle_boundary_and_transactional_commit(void) {
  fixed_point_filter_t filter = { 0 };
  fixed_point_filter_t before;
  int16_t output = 99;
  commit_context_t commit_context;

  CHECK(fixed_point_filter_init(&filter));
  before = filter;
  mock_filter_cost_reset(FIXED_POINT_FILTER_CYCLE_BUDGET - 1u);
  CHECK(!fixed_point_filter_step(
    &filter,
    mock_filter_cost_handle(),
    4096,
    &output
  ));
  CHECK(filter_state_equals(&filter, &before));
  CHECK(output == 99);
  CHECK(mock_filter_cost_begin_count() == 1u);
  CHECK(mock_filter_cost_declared_macs() == FIXED_POINT_FILTER_UNIQUE_TAPS);
  CHECK(mock_filter_cost_mac_count() == 0u);
  CHECK(mock_filter_cost_commit_count() == 0u);
  CHECK(mock_filter_cost_cycles_used() == 0u);

  mock_filter_cost_reset(FIXED_POINT_FILTER_CYCLE_BUDGET);
  mock_filter_cost_set_commit_failure(true);
  commit_context = (commit_context_t) {
    .filter = &filter,
    .expected_filter = before,
    .output = &output,
    .expected_output = output,
  };
  mock_filter_cost_set_commit_validator(
    state_is_unpublished,
    &commit_context
  );
  CHECK(!fixed_point_filter_step(
    &filter,
    mock_filter_cost_handle(),
    4096,
    &output
  ));
  CHECK(filter_state_equals(&filter, &before));
  CHECK(output == 99);
  CHECK(mock_filter_cost_mac_count() == FIXED_POINT_FILTER_UNIQUE_TAPS);
  CHECK(mock_filter_cost_commit_count() == 1u);
  CHECK(mock_filter_cost_cycles_used() == 0u);
  CHECK(!mock_filter_cost_active());
  CHECK(mock_filter_cost_commit_validated());

  mock_filter_cost_reset(FIXED_POINT_FILTER_CYCLE_BUDGET);
  mock_filter_cost_set_commit_validator(
    state_is_unpublished,
    &commit_context
  );
  CHECK(fixed_point_filter_step(
    &filter,
    mock_filter_cost_handle(),
    4096,
    &output
  ));
  CHECK(output == -128);
  CHECK(filter.history[0] == 4096);
  CHECK(mock_filter_cost_commit_validated());
  CHECK(mock_filter_cost_cycles_used() == FIXED_POINT_FILTER_CYCLE_BUDGET);
  CHECK(mock_filter_cost_mac_count() == FIXED_POINT_FILTER_UNIQUE_TAPS);
  CHECK(mock_filter_cost_sample_sum(0u) == 4096);
  CHECK(
    mock_filter_cost_coefficient(0u) == FIXED_POINT_FILTER_COEFF_0_Q15
  );
  CHECK(
    mock_filter_cost_coefficient(1u) == FIXED_POINT_FILTER_COEFF_1_Q15
  );
  CHECK(
    mock_filter_cost_coefficient(2u) == FIXED_POINT_FILTER_COEFF_2_Q15
  );
  CHECK(
    mock_filter_cost_coefficient(3u) == FIXED_POINT_FILTER_COEFF_3_Q15
  );
  return true;
}

static bool test_complete_symmetric_mac_trace(void) {
  fixed_point_filter_t filter = {
    .history = {
      INT16_C(30000),
      -INT16_C(30000),
      INT16_C(20000),
      -INT16_C(20000),
      INT16_C(10000),
      INT16_C(30000),
    },
    .initialized = true,
  };
  const fixed_point_filter_t before = filter;
  const int32_t expected_sums[] = {
    INT32_C(42345),
    INT32_C(40000),
    -INT32_C(50000),
    INT32_C(20000),
  };
  const int16_t expected_coefficients[] = {
    FIXED_POINT_FILTER_COEFF_0_Q15,
    FIXED_POINT_FILTER_COEFF_1_Q15,
    FIXED_POINT_FILTER_COEFF_2_Q15,
    FIXED_POINT_FILTER_COEFF_3_Q15,
  };
  int16_t output = 0;

  mock_filter_cost_reset(FIXED_POINT_FILTER_CYCLE_BUDGET);
  CHECK(fixed_point_filter_step(
    &filter,
    mock_filter_cost_handle(),
    INT16_C(12345),
    &output
  ));
  CHECK(output == ideal_step(before.history, INT16_C(12345)));
  CHECK(mock_filter_cost_begin_count() == 1u);
  CHECK(mock_filter_cost_declared_macs() == FIXED_POINT_FILTER_UNIQUE_TAPS);
  CHECK(mock_filter_cost_mac_count() == FIXED_POINT_FILTER_UNIQUE_TAPS);
  CHECK(mock_filter_cost_commit_count() == 1u);
  CHECK(mock_filter_cost_cycles_used() == FIXED_POINT_FILTER_CYCLE_BUDGET);
  for (size_t index = 0u; index < FIXED_POINT_FILTER_UNIQUE_TAPS; index++) {
    CHECK(mock_filter_cost_sample_sum(index) == expected_sums[index]);
    CHECK(
      mock_filter_cost_coefficient(index) == expected_coefficients[index]
    );
  }
  CHECK(filter.history[0] == INT16_C(12345));
  for (size_t index = 1u;
    index < FIXED_POINT_FILTER_HISTORY_SAMPLES;
    index++) {
    CHECK(filter.history[index] == before.history[index - 1u]);
  }
  return true;
}

static bool test_impulse_response_and_signed_ties(void) {
  const int16_t expected[] = {
    -512,
    1024,
    2048,
    11264,
    2048,
    1024,
    -512,
  };
  fixed_point_filter_t filter = { 0 };
  int16_t output = 0;

  CHECK(fixed_point_filter_init(&filter));
  mock_filter_cost_reset(
    (uint32_t)(sizeof(expected) / sizeof(expected[0])) *
      FIXED_POINT_FILTER_CYCLE_BUDGET
  );
  for (size_t index = 0u;
    index < sizeof(expected) / sizeof(expected[0]);
    index++) {
    const int16_t input = index == 0u ? INT16_C(16384) : 0;
    CHECK(fixed_point_filter_step(
      &filter,
      mock_filter_cost_handle(),
      input,
      &output
    ));
    CHECK(output == expected[index]);
  }
  CHECK(
    mock_filter_cost_cycles_used() ==
      (uint32_t)(sizeof(expected) / sizeof(expected[0])) *
        FIXED_POINT_FILTER_CYCLE_BUDGET
  );

  CHECK(fixed_point_filter_init(&filter));
  mock_filter_cost_reset(FIXED_POINT_FILTER_CYCLE_BUDGET);
  CHECK(fixed_point_filter_step(
    &filter,
    mock_filter_cost_handle(),
    INT16_C(16),
    &output
  ));
  CHECK(output == -1);

  CHECK(fixed_point_filter_init(&filter));
  mock_filter_cost_reset(FIXED_POINT_FILTER_CYCLE_BUDGET);
  CHECK(fixed_point_filter_step(
    &filter,
    mock_filter_cost_handle(),
    -INT16_C(16),
    &output
  ));
  CHECK(output == 1);
  return true;
}

static bool run_saturation_sequence(bool positive) {
  const int16_t high = positive ? INT16_MAX : INT16_MIN;
  const int16_t low = positive ? INT16_MIN : INT16_MAX;
  const int16_t samples[] = {
    low,
    high,
    high,
    high,
    high,
    high,
    low,
  };
  fixed_point_filter_t filter = { 0 };
  int16_t output = 0;

  CHECK(fixed_point_filter_init(&filter));
  mock_filter_cost_reset(
    (uint32_t)(sizeof(samples) / sizeof(samples[0])) *
      FIXED_POINT_FILTER_CYCLE_BUDGET
  );
  for (size_t index = 0u;
    index < sizeof(samples) / sizeof(samples[0]);
    index++) {
    CHECK(fixed_point_filter_step(
      &filter,
      mock_filter_cost_handle(),
      samples[index],
      &output
    ));
  }
  CHECK(output == high);
  return true;
}

static bool test_saturation_and_error_budget(void) {
  fixed_point_filter_t filter = { 0 };
  int16_t ideal_history[FIXED_POINT_FILTER_HISTORY_SAMPLES] = { 0 };
  uint32_t generator = UINT32_C(0x13579bdf);
  int16_t output = 0;

  CHECK(run_saturation_sequence(true));
  CHECK(run_saturation_sequence(false));
  CHECK(fixed_point_filter_init(&filter));
  mock_filter_cost_reset(UINT32_C(40) * FIXED_POINT_FILTER_CYCLE_BUDGET);
  for (size_t index = 0u; index < 40u; index++) {
    uint16_t bits;
    int32_t signed_value;
    int16_t input;
    int16_t expected;
    int32_t error;

    generator = generator * UINT32_C(1664525) + UINT32_C(1013904223);
    bits = (uint16_t)(generator >> 16);
    signed_value = bits <= (uint16_t)INT16_MAX
      ? (int32_t)bits
      : (int32_t)bits - INT32_C(65536);
    input = (int16_t)signed_value;
    expected = ideal_step(ideal_history, input);
    CHECK(fixed_point_filter_step(
      &filter,
      mock_filter_cost_handle(),
      input,
      &output
    ));
    error = (int32_t)output - expected;
    if (error < 0) error = -error;
    CHECK(error <= (int32_t)FIXED_POINT_FILTER_ERROR_BUDGET_LSB);
    CHECK(output == expected);
    shift_history(ideal_history, input);
  }
  CHECK(
    mock_filter_cost_cycles_used() ==
      UINT32_C(40) * FIXED_POINT_FILTER_CYCLE_BUDGET
  );
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "initialization and invalid calls", test_initialization_and_invalid_calls },
    { "cycle boundary and transactional commit", test_cycle_boundary_and_transactional_commit },
    { "complete symmetric MAC trace", test_complete_symmetric_mac_trace },
    { "impulse response and signed ties", test_impulse_response_and_signed_ties },
    { "saturation and error budget", test_saturation_and_error_budget },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) return 1;
    printf("ok - %s\n", tests[index].name);
  }
  return 0;
}

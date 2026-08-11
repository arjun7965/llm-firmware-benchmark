#include "fixed_point_filter.h"

static int64_t divide_q15_rounded(int64_t value) {
  const int64_t half = (int64_t)FIXED_POINT_FILTER_Q15_SCALE / 2;

  if (value >= 0) {
    return (value + half) / (int64_t)FIXED_POINT_FILTER_Q15_SCALE;
  }
  return (value - half) / (int64_t)FIXED_POINT_FILTER_Q15_SCALE;
}

static int16_t saturate_int16(int64_t value) {
  if (value > INT16_MAX) return INT16_MAX;
  if (value < INT16_MIN) return INT16_MIN;
  return (int16_t)value;
}

bool fixed_point_filter_init(fixed_point_filter_t *filter) {
  if (filter == NULL) return false;

  for (size_t index = 0u;
    index < FIXED_POINT_FILTER_HISTORY_SAMPLES;
    index++) {
    filter->history[index] = 0;
  }
  filter->initialized = true;
  return true;
}

bool fixed_point_filter_step(
  fixed_point_filter_t *filter,
  filter_cost_model_t *cost,
  int16_t input_q15,
  int16_t *output_q15
) {
  int64_t accumulator = 0;
  int64_t rounded;
  int16_t result;
  int32_t pair0;
  int32_t pair1;
  int32_t pair2;

  if (
    filter == NULL ||
    !filter->initialized ||
    cost == NULL ||
    output_q15 == NULL
  ) {
    return false;
  }
  pair0 = (int32_t)input_q15 + filter->history[5];
  pair1 = (int32_t)filter->history[0] + filter->history[4];
  pair2 = (int32_t)filter->history[1] + filter->history[3];
  if (!filter_cost_begin_step(cost, FIXED_POINT_FILTER_UNIQUE_TAPS)) {
    return false;
  }

  filter_cost_mac_q15(
    cost,
    &accumulator,
    pair0,
    FIXED_POINT_FILTER_COEFF_0_Q15
  );
  filter_cost_mac_q15(
    cost,
    &accumulator,
    pair1,
    FIXED_POINT_FILTER_COEFF_1_Q15
  );
  filter_cost_mac_q15(
    cost,
    &accumulator,
    pair2,
    FIXED_POINT_FILTER_COEFF_2_Q15
  );
  filter_cost_mac_q15(
    cost,
    &accumulator,
    filter->history[2],
    FIXED_POINT_FILTER_COEFF_3_Q15
  );

  rounded = divide_q15_rounded(accumulator);
  result = saturate_int16(rounded);
  if (!filter_cost_commit_step(cost)) return false;

  for (size_t index = FIXED_POINT_FILTER_HISTORY_SAMPLES - 1u;
    index > 0u;
    index--) {
    filter->history[index] = filter->history[index - 1u];
  }
  filter->history[0] = input_q15;
  *output_q15 = result;
  return true;
}

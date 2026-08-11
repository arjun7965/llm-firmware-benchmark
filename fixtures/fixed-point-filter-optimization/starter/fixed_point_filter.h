#ifndef FIXED_POINT_FILTER_H
#define FIXED_POINT_FILTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_filter_cost.h"

#define FIXED_POINT_FILTER_Q15_SCALE UINT32_C(32768)
#define FIXED_POINT_FILTER_HISTORY_SAMPLES 6u
#define FIXED_POINT_FILTER_UNIQUE_TAPS UINT32_C(4)
#define FIXED_POINT_FILTER_FIXED_CYCLES UINT32_C(12)
#define FIXED_POINT_FILTER_MAC_CYCLES UINT32_C(4)
#define FIXED_POINT_FILTER_CYCLE_BUDGET UINT32_C(28)
#define FIXED_POINT_FILTER_ERROR_BUDGET_LSB UINT32_C(1)

#define FIXED_POINT_FILTER_COEFF_0_Q15 (-INT16_C(1024))
#define FIXED_POINT_FILTER_COEFF_1_Q15 INT16_C(2048)
#define FIXED_POINT_FILTER_COEFF_2_Q15 INT16_C(4096)
#define FIXED_POINT_FILTER_COEFF_3_Q15 INT16_C(22528)

typedef struct {
  int16_t history[FIXED_POINT_FILTER_HISTORY_SAMPLES];
  bool initialized;
} fixed_point_filter_t;

bool fixed_point_filter_init(fixed_point_filter_t *filter);
bool fixed_point_filter_step(
  fixed_point_filter_t *filter,
  filter_cost_model_t *cost,
  int16_t input_q15,
  int16_t *output_q15
);

#endif

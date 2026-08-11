#ifndef FIXTURE_FILTER_COST_H
#define FIXTURE_FILTER_COST_H

#include <stdbool.h>
#include <stdint.h>

typedef struct filter_cost_model filter_cost_model_t;

bool filter_cost_begin_step(
  filter_cost_model_t *cost,
  uint32_t mac_count
);
void filter_cost_mac_q15(
  filter_cost_model_t *cost,
  int64_t *accumulator,
  int32_t sample_sum_q15,
  int16_t coefficient_q15
);
bool filter_cost_commit_step(filter_cost_model_t *cost);

#endif

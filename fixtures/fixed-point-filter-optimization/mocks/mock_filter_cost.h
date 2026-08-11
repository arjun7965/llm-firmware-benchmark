#ifndef MOCK_FILTER_COST_H
#define MOCK_FILTER_COST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_filter_cost.h"

filter_cost_model_t *mock_filter_cost_handle(void);
void mock_filter_cost_reset(uint32_t cycle_capacity);
void mock_filter_cost_set_commit_failure(bool fail);
uint32_t mock_filter_cost_cycles_used(void);
size_t mock_filter_cost_begin_count(void);
size_t mock_filter_cost_mac_count(void);
size_t mock_filter_cost_commit_count(void);
uint32_t mock_filter_cost_declared_macs(void);
int32_t mock_filter_cost_sample_sum(size_t index);
int16_t mock_filter_cost_coefficient(size_t index);
bool mock_filter_cost_active(void);

#endif

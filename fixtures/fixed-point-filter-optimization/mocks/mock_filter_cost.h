#ifndef MOCK_FILTER_COST_H
#define MOCK_FILTER_COST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_filter_cost.h"

typedef bool (*mock_filter_cost_commit_validator_t)(const void *context);

filter_cost_model_t *mock_filter_cost_handle(void);
void mock_filter_cost_reset(uint32_t cycle_capacity);
void mock_filter_cost_set_commit_failure(bool fail);
void mock_filter_cost_set_commit_validator(
  mock_filter_cost_commit_validator_t validator,
  const void *context
);
uint32_t mock_filter_cost_cycles_used(void);
size_t mock_filter_cost_begin_count(void);
size_t mock_filter_cost_mac_count(void);
size_t mock_filter_cost_commit_count(void);
uint32_t mock_filter_cost_declared_macs(void);
int32_t mock_filter_cost_sample_sum(size_t index);
int16_t mock_filter_cost_coefficient(size_t index);
bool mock_filter_cost_active(void);
bool mock_filter_cost_commit_validated(void);
bool mock_filter_cost_invalid_access(void);

#endif

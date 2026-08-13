#include "mock_filter_cost.h"

#include <limits.h>

#include "fixed_point_filter.h"

#define MOCK_FILTER_COST_MAX_MACS 256u

struct filter_cost_model {
  uint32_t cycle_capacity;
  uint32_t cycles_used;
  uint32_t pending_cycles;
  uint32_t declared_macs;
  uint32_t active_macs;
  size_t begin_count;
  size_t mac_count;
  size_t commit_count;
  int32_t sample_sums[MOCK_FILTER_COST_MAX_MACS];
  int16_t coefficients[MOCK_FILTER_COST_MAX_MACS];
  bool active;
  bool violation;
  bool fail_commit;
  mock_filter_cost_commit_validator_t commit_validator;
  const void *commit_context;
  bool commit_validated;
  bool invalid_access;
};

static struct filter_cost_model model;

filter_cost_model_t *mock_filter_cost_handle(void) { return &model; }

void mock_filter_cost_reset(uint32_t cycle_capacity) {
  model = (struct filter_cost_model) {
    .cycle_capacity = cycle_capacity,
  };
}

void mock_filter_cost_set_commit_failure(bool fail) {
  model.fail_commit = fail;
}

void mock_filter_cost_set_commit_validator(
  mock_filter_cost_commit_validator_t validator,
  const void *context
) {
  model.commit_validator = validator;
  model.commit_context = context;
  model.commit_validated = false;
}

uint32_t mock_filter_cost_cycles_used(void) { return model.cycles_used; }
size_t mock_filter_cost_begin_count(void) { return model.begin_count; }
size_t mock_filter_cost_mac_count(void) { return model.mac_count; }
size_t mock_filter_cost_commit_count(void) { return model.commit_count; }
uint32_t mock_filter_cost_declared_macs(void) {
  return model.declared_macs;
}
int32_t mock_filter_cost_sample_sum(size_t index) {
  return index < model.mac_count ? model.sample_sums[index] : 0;
}
int16_t mock_filter_cost_coefficient(size_t index) {
  return index < model.mac_count ? model.coefficients[index] : 0;
}
bool mock_filter_cost_active(void) { return model.active; }
bool mock_filter_cost_commit_validated(void) {
  return model.commit_validated;
}
bool mock_filter_cost_invalid_access(void) { return model.invalid_access; }

bool filter_cost_begin_step(
  filter_cost_model_t *cost,
  uint32_t mac_count
) {
  uint32_t required_cycles;

  if (cost != &model) {
    model.invalid_access = true;
    return false;
  }
  model.begin_count++;
  model.declared_macs = mac_count;
  if (
    model.active ||
    mac_count >
      (UINT32_MAX - FIXED_POINT_FILTER_FIXED_CYCLES) /
        FIXED_POINT_FILTER_MAC_CYCLES
  ) {
    return false;
  }
  required_cycles = FIXED_POINT_FILTER_FIXED_CYCLES +
    mac_count * FIXED_POINT_FILTER_MAC_CYCLES;
  if (required_cycles > model.cycle_capacity - model.cycles_used) {
    return false;
  }
  model.pending_cycles = required_cycles;
  model.active_macs = 0u;
  model.violation = false;
  model.active = true;
  return true;
}

void filter_cost_mac_q15(
  filter_cost_model_t *cost,
  int64_t *accumulator,
  int32_t sample_sum_q15,
  int16_t coefficient_q15
) {
  if (cost != &model) {
    model.invalid_access = true;
    return;
  }
  if (model.mac_count < MOCK_FILTER_COST_MAX_MACS) {
    model.sample_sums[model.mac_count] = sample_sum_q15;
    model.coefficients[model.mac_count] = coefficient_q15;
  } else {
    model.violation = true;
  }
  model.mac_count++;
  model.active_macs++;
  if (
    !model.active ||
    model.active_macs > model.declared_macs ||
    accumulator == NULL
  ) {
    model.violation = true;
    return;
  }
  *accumulator += (int64_t)sample_sum_q15 * coefficient_q15;
}

bool filter_cost_commit_step(filter_cost_model_t *cost) {
  bool success;

  if (cost != &model) {
    model.invalid_access = true;
    return false;
  }
  model.commit_count++;
  if (model.commit_validator != NULL) {
    model.commit_validated = true;
    if (!model.commit_validator(model.commit_context)) {
      model.violation = true;
    }
  }
  success = model.active &&
    !model.violation &&
    model.active_macs == model.declared_macs &&
    !model.fail_commit;
  model.active = false;
  if (success) model.cycles_used += model.pending_cycles;
  model.pending_cycles = 0u;
  return success;
}

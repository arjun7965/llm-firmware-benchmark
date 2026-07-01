#include "mock_hal.h"

#include <string.h>

#include "fixture_hal.h"

#define MOCK_MAX_PLANS 16u
#define MOCK_MAX_STARTS 32u
#define MOCK_CONTROLLER_TIMEOUT_MS UINT32_C(25)

typedef struct {
  bool accept;
  uint32_t completion_delay_ms;
  bool success;
  uint8_t bytes[2];
} transfer_plan_t;

static uint32_t now_ms;
static transfer_plan_t plans[MOCK_MAX_PLANS];
static size_t plan_count;
static size_t next_plan;
static uint32_t start_times[MOCK_MAX_STARTS];
static size_t start_count;
static bool overlap_detected;
static bool active;
static bool completed;
static bool completed_ok;
static uint32_t active_started_ms;
static transfer_plan_t active_plan;
static uint8_t *active_destination;
static size_t active_length;
static uint8_t last_address;
static uint8_t last_register;
static size_t last_length;

static uint32_t elapsed_since(uint32_t start_ms) {
  return now_ms - start_ms;
}

void mock_hal_reset(uint32_t initial_now_ms) {
  now_ms = initial_now_ms;
  memset(plans, 0, sizeof(plans));
  plan_count = 0u;
  next_plan = 0u;
  memset(start_times, 0, sizeof(start_times));
  start_count = 0u;
  overlap_detected = false;
  active = false;
  completed = false;
  completed_ok = false;
  active_started_ms = 0u;
  memset(&active_plan, 0, sizeof(active_plan));
  active_destination = NULL;
  active_length = 0u;
  last_address = 0u;
  last_register = 0u;
  last_length = 0u;
}

bool mock_hal_plan_transfer(
  bool accept,
  uint32_t completion_delay_ms,
  bool success,
  uint8_t first_byte,
  uint8_t second_byte
) {
  if (plan_count >= MOCK_MAX_PLANS) return false;
  plans[plan_count] = (transfer_plan_t) {
    .accept = accept,
    .completion_delay_ms = completion_delay_ms,
    .success = success,
    .bytes = { first_byte, second_byte },
  };
  plan_count++;
  return true;
}

void mock_hal_advance(uint32_t delta_ms) {
  now_ms += delta_ms;
}

size_t mock_hal_start_count(void) {
  return start_count;
}

uint32_t mock_hal_start_time(size_t index) {
  return index < start_count && index < MOCK_MAX_STARTS
    ? start_times[index]
    : 0u;
}

bool mock_hal_overlap_detected(void) {
  return overlap_detected;
}

bool mock_hal_in_flight(void) {
  return active;
}

uint8_t mock_hal_last_address(void) {
  return last_address;
}

uint8_t mock_hal_last_register(void) {
  return last_register;
}

size_t mock_hal_last_length(void) {
  return last_length;
}

uint32_t millis(void) {
  return now_ms;
}

bool i2c_start_read(
  uint8_t address,
  uint8_t reg,
  uint8_t *destination,
  size_t length
) {
  if (start_count < MOCK_MAX_STARTS) {
    start_times[start_count] = now_ms;
  }
  start_count++;
  last_address = address;
  last_register = reg;
  last_length = length;

  if (active) {
    if (elapsed_since(active_started_ms) < MOCK_CONTROLLER_TIMEOUT_MS) {
      overlap_detected = true;
      return false;
    }
    active = false;
  }
  completed = false;
  completed_ok = false;

  if (next_plan >= plan_count) return false;
  active_plan = plans[next_plan++];
  if (!active_plan.accept) return false;

  active = true;
  active_started_ms = now_ms;
  active_destination = destination;
  active_length = length;
  return true;
}

bool i2c_done(void) {
  if (completed) return true;
  if (!active) return false;

  const uint32_t elapsed = elapsed_since(active_started_ms);
  if (elapsed >= active_plan.completion_delay_ms) {
    if (active_destination != NULL) {
      if (active_length > 0u) active_destination[0] = active_plan.bytes[0];
      if (active_length > 1u) active_destination[1] = active_plan.bytes[1];
    }
    active = false;
    completed = true;
    completed_ok = active_plan.success;
    return true;
  }
  if (elapsed >= MOCK_CONTROLLER_TIMEOUT_MS) {
    active = false;
  }
  return false;
}

bool i2c_ok(void) {
  return completed && completed_ok;
}

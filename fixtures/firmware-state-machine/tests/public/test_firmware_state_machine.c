#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "firmware_state_machine.h"
#include "mock_hal.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
              __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

static bool plan(
  bool accept,
  uint32_t delay_ms,
  bool success,
  uint8_t first,
  uint8_t second
) {
  return mock_hal_plan_transfer(
    accept,
    delay_ms,
    success,
    first,
    second
  );
}

static bool test_success_and_poll_interval(void) {
  temperature_sample_t sample;
  mock_hal_reset(UINT32_C(100));
  CHECK(plan(true, UINT32_C(5), true, UINT8_C(0x12), UINT8_C(0x34)));
  temperature_task_init();

  temperature_task_step();
  CHECK(mock_hal_start_count() == 0u);
  mock_hal_advance(UINT32_C(999));
  temperature_task_step();
  CHECK(mock_hal_start_count() == 0u);
  mock_hal_advance(UINT32_C(1));
  temperature_task_step();
  CHECK(mock_hal_start_count() == 1u);
  CHECK(mock_hal_in_flight());
  CHECK(mock_hal_last_address() == TEMPERATURE_SENSOR_ADDRESS);
  CHECK(mock_hal_last_register() == TEMPERATURE_SENSOR_REGISTER);
  CHECK(mock_hal_last_length() == TEMPERATURE_SAMPLE_SIZE);

  temperature_task_step();
  CHECK(mock_hal_start_count() == 1u);
  CHECK(!mock_hal_overlap_detected());
  mock_hal_advance(UINT32_C(5));
  temperature_task_step();
  CHECK(temperature_task_latest(&sample));
  CHECK(sample.valid);
  CHECK(sample.bytes[0] == UINT8_C(0x12));
  CHECK(sample.bytes[1] == UINT8_C(0x34));
  CHECK(sample.timestamp_ms == UINT32_C(1105));
  return true;
}

static bool test_failed_transfer_retries_after_backoff(void) {
  temperature_sample_t sample;
  mock_hal_reset(0u);
  CHECK(plan(true, UINT32_C(1), false, 0u, 0u));
  CHECK(plan(true, UINT32_C(2), true, UINT8_C(0xAA), UINT8_C(0x55)));
  temperature_task_init();

  mock_hal_advance(UINT32_C(1000));
  temperature_task_step();
  mock_hal_advance(UINT32_C(1));
  temperature_task_step();
  mock_hal_advance(UINT32_C(9));
  temperature_task_step();
  CHECK(mock_hal_start_count() == 1u);
  mock_hal_advance(UINT32_C(1));
  temperature_task_step();
  CHECK(mock_hal_start_count() == 2u);
  CHECK(mock_hal_start_time(0u) == UINT32_C(1000));
  CHECK(mock_hal_start_time(1u) == UINT32_C(1011));

  mock_hal_advance(UINT32_C(2));
  temperature_task_step();
  CHECK(temperature_task_latest(&sample));
  CHECK(sample.bytes[0] == UINT8_C(0xAA));
  CHECK(sample.bytes[1] == UINT8_C(0x55));
  CHECK(sample.timestamp_ms == UINT32_C(1013));
  CHECK(!mock_hal_overlap_detected());
  return true;
}

static bool test_rejected_start_counts_as_an_attempt(void) {
  temperature_sample_t sample;
  mock_hal_reset(0u);
  CHECK(plan(false, 0u, false, 0u, 0u));
  CHECK(plan(true, 0u, true, UINT8_C(0x01), UINT8_C(0x02)));
  temperature_task_init();

  mock_hal_advance(UINT32_C(1000));
  temperature_task_step();
  CHECK(mock_hal_start_count() == 1u);
  mock_hal_advance(UINT32_C(10));
  temperature_task_step();
  CHECK(mock_hal_start_count() == 2u);
  temperature_task_step();
  CHECK(temperature_task_latest(&sample));
  CHECK(sample.timestamp_ms == UINT32_C(1010));
  return true;
}

static bool test_timeout_exhaustion_and_next_cycle(void) {
  temperature_sample_t sample;
  mock_hal_reset(0u);
  CHECK(plan(true, UINT32_C(100), true, 1u, 1u));
  CHECK(plan(true, UINT32_C(100), true, 2u, 2u));
  CHECK(plan(true, UINT32_C(100), true, 3u, 3u));
  CHECK(plan(true, 0u, true, UINT8_C(0xDE), UINT8_C(0xAD)));
  temperature_task_init();

  mock_hal_advance(UINT32_C(1000));
  temperature_task_step();
  for (size_t attempt = 0u; attempt < 3u; attempt++) {
    mock_hal_advance(UINT32_C(25));
    temperature_task_step();
    if (attempt < 2u) {
      mock_hal_advance(UINT32_C(10));
      temperature_task_step();
    }
  }
  CHECK(mock_hal_start_count() == 3u);
  CHECK(!mock_hal_overlap_detected());
  CHECK(!temperature_task_latest(&sample));

  mock_hal_advance(UINT32_C(999));
  temperature_task_step();
  CHECK(mock_hal_start_count() == 3u);
  mock_hal_advance(UINT32_C(1));
  temperature_task_step();
  CHECK(mock_hal_start_count() == 4u);
  temperature_task_step();
  CHECK(temperature_task_latest(&sample));
  CHECK(sample.bytes[0] == UINT8_C(0xDE));
  CHECK(sample.bytes[1] == UINT8_C(0xAD));
  return true;
}

static bool test_timer_wraparound(void) {
  temperature_sample_t sample;
  mock_hal_reset(UINT32_MAX - UINT32_C(499));
  CHECK(plan(true, 0u, true, UINT8_C(0xFE), UINT8_C(0xDC)));
  temperature_task_init();

  temperature_task_step();
  CHECK(mock_hal_start_count() == 0u);
  mock_hal_advance(UINT32_C(999));
  temperature_task_step();
  CHECK(mock_hal_start_count() == 0u);
  mock_hal_advance(UINT32_C(1));
  temperature_task_step();
  CHECK(mock_hal_start_count() == 1u);
  CHECK(mock_hal_start_time(0u) == UINT32_C(500));
  temperature_task_step();
  CHECK(temperature_task_latest(&sample));
  CHECK(sample.timestamp_ms == UINT32_C(500));
  return true;
}

static bool test_failed_cycle_preserves_latest_sample(void) {
  temperature_sample_t sample;
  mock_hal_reset(0u);
  CHECK(plan(true, 0u, true, UINT8_C(0x11), UINT8_C(0x22)));
  CHECK(plan(true, UINT32_C(100), true, 0u, 0u));
  CHECK(plan(true, UINT32_C(100), true, 0u, 0u));
  CHECK(plan(true, UINT32_C(100), true, 0u, 0u));
  temperature_task_init();

  mock_hal_advance(UINT32_C(1000));
  temperature_task_step();
  temperature_task_step();
  CHECK(temperature_task_latest(&sample));
  CHECK(sample.timestamp_ms == UINT32_C(1000));

  mock_hal_advance(UINT32_C(1000));
  temperature_task_step();
  for (size_t attempt = 0u; attempt < 3u; attempt++) {
    mock_hal_advance(UINT32_C(25));
    temperature_task_step();
    if (attempt < 2u) {
      mock_hal_advance(UINT32_C(10));
      temperature_task_step();
    }
  }

  CHECK(temperature_task_latest(&sample));
  CHECK(sample.bytes[0] == UINT8_C(0x11));
  CHECK(sample.bytes[1] == UINT8_C(0x22));
  CHECK(sample.timestamp_ms == UINT32_C(1000));
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "success and poll interval", test_success_and_poll_interval },
    { "failure retry", test_failed_transfer_retries_after_backoff },
    { "rejected start", test_rejected_start_counts_as_an_attempt },
    { "timeout exhaustion", test_timeout_exhaustion_and_next_cycle },
    { "timer wraparound", test_timer_wraparound },
    { "sample preservation", test_failed_cycle_preserves_latest_sample },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) return 1;
    printf("ok - %s\n", tests[index].name);
  }
  return 0;
}

#include "firmware_state_machine.h"

#include <stddef.h>

#define POLL_INTERVAL_MS UINT32_C(1000)
#define TRANSACTION_TIMEOUT_MS UINT32_C(25)
#define RETRY_BACKOFF_MS UINT32_C(10)
#define MAX_ATTEMPTS 3u

typedef enum {
  STATE_WAIT_POLL,
  STATE_WAIT_TRANSFER,
  STATE_WAIT_BACKOFF,
} task_state_t;

static task_state_t state;
static uint32_t deadline_ms;
static uint32_t transfer_started_ms;
static unsigned int attempts;
static uint8_t pending_bytes[TEMPERATURE_SAMPLE_SIZE];
static temperature_sample_t latest_sample;

static bool deadline_reached(uint32_t now, uint32_t deadline) {
  return (int32_t) (now - deadline) >= 0;
}

static void finish_cycle(uint32_t now) {
  attempts = 0u;
  deadline_ms = now + POLL_INTERVAL_MS;
  state = STATE_WAIT_POLL;
}

static void handle_attempt_failure(uint32_t now) {
  if (attempts >= MAX_ATTEMPTS) {
    finish_cycle(now);
    return;
  }
  deadline_ms = now + RETRY_BACKOFF_MS;
  state = STATE_WAIT_BACKOFF;
}

static void start_attempt(uint32_t now) {
  attempts++;
  if (i2c_start_read(
    TEMPERATURE_SENSOR_ADDRESS,
    TEMPERATURE_SENSOR_REGISTER,
    pending_bytes,
    TEMPERATURE_SAMPLE_SIZE
  )) {
    transfer_started_ms = now;
    state = STATE_WAIT_TRANSFER;
    return;
  }
  handle_attempt_failure(now);
}

void temperature_task_init(void) {
  state = STATE_WAIT_POLL;
  deadline_ms = millis() + POLL_INTERVAL_MS;
  transfer_started_ms = 0u;
  attempts = 0u;
  pending_bytes[0] = 0u;
  pending_bytes[1] = 0u;
  latest_sample = (temperature_sample_t) {
    .bytes = { 0u, 0u },
    .timestamp_ms = 0u,
    .valid = false,
  };
}

void temperature_task_step(void) {
  const uint32_t now = millis();

  switch (state) {
    case STATE_WAIT_POLL:
      if (deadline_reached(now, deadline_ms)) start_attempt(now);
      break;

    case STATE_WAIT_TRANSFER:
      if (i2c_done()) {
        if (i2c_ok()) {
          latest_sample.bytes[0] = pending_bytes[0];
          latest_sample.bytes[1] = pending_bytes[1];
          latest_sample.timestamp_ms = now;
          latest_sample.valid = true;
          finish_cycle(now);
        } else {
          handle_attempt_failure(now);
        }
      } else if ((uint32_t) (now - transfer_started_ms) >=
                 TRANSACTION_TIMEOUT_MS) {
        handle_attempt_failure(now);
      }
      break;

    case STATE_WAIT_BACKOFF:
      if (deadline_reached(now, deadline_ms)) start_attempt(now);
      break;
  }
}

bool temperature_task_latest(temperature_sample_t *output) {
  if (output == NULL || !latest_sample.valid) return false;
  *output = latest_sample;
  return true;
}

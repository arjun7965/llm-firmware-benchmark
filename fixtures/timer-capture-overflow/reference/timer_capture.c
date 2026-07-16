#include <stddef.h>

#include "timer_capture.h"

static bool timer_capture_is_ready(const timer_capture_t *driver) {
  return driver != NULL && driver->initialized && driver->timer != NULL;
}

static uint32_t timestamp_from_low_word(
  const timer_capture_t *driver,
  uint16_t low_word,
  bool overflow_observed
) {
  uint32_t high_word = driver->overflow_ticks;

  if (overflow_observed && low_word < TIMER1_COUNTER_HALF_RANGE) {
    high_word += TIMER1_COUNTER_MODULUS;
  }
  return high_word | (uint32_t)low_word;
}

bool timer_capture_init(
  timer_capture_t *driver,
  volatile timer1_registers_t *timer
) {
  if (driver == NULL || timer == NULL) return false;

  timer1_write_control(timer, 0u);
  timer1_write_status_clear(timer, TIMER1_STATUS_ALL);
  timer1_write_compare(timer, 0u);
  timer1_write_control(timer, TIMER1_CONTROL_READY);
  *driver = (timer_capture_t) {
    .timer = timer,
    .initialized = true,
  };
  return true;
}

bool timer_capture_arm_compare(timer_capture_t *driver, uint16_t delay_ticks) {
  uint16_t count;
  uint16_t compare_ticks;
  uint32_t irq_state;
  bool armed = false;

  if (
    !timer_capture_is_ready(driver) || delay_ticks == 0u ||
    delay_ticks > TIMER_CAPTURE_MAX_DELAY
  ) {
    return false;
  }

  irq_state = timer_capture_irq_save_disable();
  if (!driver->compare_armed && !driver->compare_pending) {
    if ((timer1_read_status(driver->timer) & TIMER1_STATUS_ALL) == 0u) {
      count = timer1_read_count(driver->timer);
      compare_ticks = (uint16_t)(count + delay_ticks);
      timer1_write_compare(driver->timer, compare_ticks);
      timer1_write_control(driver->timer, TIMER1_CONTROL_COMPARE_ARMED);
      driver->compare_deadline =
        driver->overflow_ticks + (uint32_t)count + (uint32_t)delay_ticks;
      driver->compare_ticks = compare_ticks;
      driver->compare_armed = true;
      armed = true;
    }
  }
  timer_capture_irq_restore(irq_state);
  return armed;
}

void timer_capture_irq(timer_capture_t *driver) {
  uint16_t captured_low_word = 0u;
  uint32_t captured_timestamp;
  uint32_t compare_timestamp;
  uint32_t observed;
  bool compare_fired = false;
  bool overflow_observed;

  if (!timer_capture_is_ready(driver)) return;

  observed = timer1_read_status(driver->timer) & TIMER1_STATUS_ALL;
  if (observed == 0u) return;

  overflow_observed = (observed & TIMER1_STATUS_OVERFLOW) != 0u;
  if ((observed & TIMER1_STATUS_CAPTURE) != 0u) {
    captured_low_word = timer1_read_capture(driver->timer);
    captured_timestamp = timestamp_from_low_word(
      driver,
      captured_low_word,
      overflow_observed
    );
    if (driver->capture_pending) {
      driver->capture_overruns++;
    } else {
      driver->capture_timestamp = captured_timestamp;
      driver->capture_pending = true;
    }
  }

  if ((observed & TIMER1_STATUS_COMPARE) != 0u) {
    compare_timestamp = timestamp_from_low_word(
      driver,
      driver->compare_ticks,
      overflow_observed
    );
    if (
      driver->compare_armed &&
      compare_timestamp == driver->compare_deadline
    ) {
      driver->compare_timestamp = compare_timestamp;
      driver->compare_pending = true;
      driver->compare_armed = false;
      compare_fired = true;
    }
  }

  if (overflow_observed) driver->overflow_ticks += TIMER1_COUNTER_MODULUS;
  timer1_write_status_clear(driver->timer, observed);
  if (compare_fired) {
    timer1_write_control(driver->timer, TIMER1_CONTROL_READY);
  }
}

bool timer_capture_take_capture(timer_capture_t *driver, uint32_t *timestamp) {
  uint32_t irq_state;
  bool captured = false;

  if (!timer_capture_is_ready(driver) || timestamp == NULL) return false;

  irq_state = timer_capture_irq_save_disable();
  if (driver->capture_pending) {
    *timestamp = driver->capture_timestamp;
    driver->capture_pending = false;
    captured = true;
  } else {
    *timestamp = 0u;
  }
  timer_capture_irq_restore(irq_state);
  return captured;
}

bool timer_capture_take_compare(timer_capture_t *driver, uint32_t *timestamp) {
  uint32_t irq_state;
  bool compared = false;

  if (!timer_capture_is_ready(driver) || timestamp == NULL) return false;

  irq_state = timer_capture_irq_save_disable();
  if (driver->compare_pending) {
    *timestamp = driver->compare_timestamp;
    driver->compare_pending = false;
    compared = true;
  } else {
    *timestamp = 0u;
  }
  timer_capture_irq_restore(irq_state);
  return compared;
}

uint32_t timer_capture_take_overruns(timer_capture_t *driver) {
  uint32_t irq_state;
  uint32_t overruns;

  if (!timer_capture_is_ready(driver)) return 0u;

  irq_state = timer_capture_irq_save_disable();
  overruns = driver->capture_overruns;
  driver->capture_overruns = 0u;
  timer_capture_irq_restore(irq_state);
  return overruns;
}

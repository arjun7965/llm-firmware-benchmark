#include "timer_dma_handoff.h"

static bool timer_dma_is_ready(const timer_dma_t *driver) {
  return driver != NULL && driver->initialized && driver->timer != NULL &&
    driver->dma != NULL;
}

static void clear_active_sequence(timer_dma_t *driver) {
  driver->samples = NULL;
  driver->sample_count = 0u;
}

static void release_dma_ownership(timer_dma_t *driver) {
  timer0_write_control(driver->timer, TIMER0_CONTROL_READY);
  dma0_write_control(driver->dma, 0u);
  driver->last_compare_ticks = timer0_read_compare_active(driver->timer);
  clear_active_sequence(driver);
}

bool timer_dma_init(
  timer_dma_t *driver,
  volatile timer0_registers_t *timer,
  volatile dma0_registers_t *dma,
  uint16_t period_ticks,
  uint16_t initial_compare_ticks
) {
  if (
    driver == NULL || timer == NULL || dma == NULL ||
    period_ticks < UINT16_C(2) || period_ticks > TIMER0_MAX_PERIOD_TICKS ||
    initial_compare_ticks > period_ticks
  ) {
    return false;
  }

  timer0_write_control(timer, 0u);
  dma0_write_control(dma, 0u);
  dma0_write_status_clear(dma, DMA0_STATUS_ALL);
  timer0_write_period(timer, period_ticks);
  timer0_write_compare_shadow(timer, initial_compare_ticks);
  timer0_write_control(timer, TIMER0_CONTROL_READY);
  *driver = (timer_dma_t) {
    .timer = timer,
    .dma = dma,
    .period_ticks = period_ticks,
    .last_compare_ticks = initial_compare_ticks,
    .owner = TIMER_DMA_OWNER_CPU,
    .result = TIMER_DMA_RESULT_NONE,
    .initialized = true,
  };
  return true;
}

bool timer_dma_start(
  timer_dma_t *driver,
  const uint16_t *samples,
  size_t sample_count
) {
  uint32_t irq_state;
  bool started = false;

  if (
    !timer_dma_is_ready(driver) || samples == NULL || sample_count == 0u ||
    sample_count > TIMER_DMA_MAX_SAMPLES
  ) {
    return false;
  }
  for (size_t index = 0u; index < sample_count; index++) {
    if (samples[index] > driver->period_ticks) return false;
  }

  irq_state = timer_dma_irq_save_disable();
  if (
    driver->owner == TIMER_DMA_OWNER_CPU &&
    driver->result == TIMER_DMA_RESULT_NONE
  ) {
    dma0_write_status_clear(driver->dma, DMA0_STATUS_ALL);
    dma0_write_source(driver->dma, timer_dma_buffer_address(samples));
    dma0_write_destination(
      driver->dma,
      timer0_compare_dma_address(driver->timer)
    );
    dma0_write_count(driver->dma, (uint32_t)sample_count);
    dma0_write_control(driver->dma, DMA0_CHANNEL_CONTROL_READY);
    timer0_write_control(driver->timer, TIMER0_CONTROL_DMA_OWNED);
    driver->samples = samples;
    driver->sample_count = sample_count;
    driver->error_flags = 0u;
    driver->owner = TIMER_DMA_OWNER_DMA;
    started = true;
  }
  timer_dma_irq_restore(irq_state);
  return started;
}

bool timer_dma_abort(timer_dma_t *driver) {
  uint32_t irq_state;
  bool abort_requested = false;

  if (!timer_dma_is_ready(driver)) return false;

  irq_state = timer_dma_irq_save_disable();
  if (
    driver->owner == TIMER_DMA_OWNER_DMA &&
    driver->result == TIMER_DMA_RESULT_NONE
  ) {
    timer0_write_control(driver->timer, TIMER0_CONTROL_READY);
    dma0_write_control(driver->dma, DMA0_CHANNEL_CONTROL_ABORT);
    driver->owner = TIMER_DMA_OWNER_ABORTING;
    abort_requested = true;
  }
  timer_dma_irq_restore(irq_state);
  return abort_requested;
}

void timer_dma_irq(timer_dma_t *driver) {
  uint32_t errors;
  uint32_t observed;

  if (
    !timer_dma_is_ready(driver) ||
    (driver->owner != TIMER_DMA_OWNER_DMA &&
      driver->owner != TIMER_DMA_OWNER_ABORTING)
  ) {
    return;
  }

  observed = dma0_read_status(driver->dma) & DMA0_STATUS_ALL;
  if (observed == 0u) return;
  dma0_write_status_clear(driver->dma, observed);

  errors = observed & DMA0_STATUS_ERROR;
  if (errors != 0u) {
    release_dma_ownership(driver);
    driver->error_flags = errors;
    driver->owner = TIMER_DMA_OWNER_RECOVERY_REQUIRED;
    driver->result = TIMER_DMA_RESULT_ERROR;
    return;
  }
  if ((observed & DMA0_STATUS_ABORTED) != 0u) {
    release_dma_ownership(driver);
    driver->owner = TIMER_DMA_OWNER_RECOVERY_REQUIRED;
    driver->result = TIMER_DMA_RESULT_ABORTED;
    return;
  }
  if ((observed & DMA0_STATUS_COMPLETE) != 0u) {
    release_dma_ownership(driver);
    driver->owner = TIMER_DMA_OWNER_CPU;
    driver->result = TIMER_DMA_RESULT_COMPLETE;
  }
}

bool timer_dma_recover(timer_dma_t *driver) {
  uint32_t irq_state;
  bool recovered = false;

  if (!timer_dma_is_ready(driver)) return false;

  irq_state = timer_dma_irq_save_disable();
  if (
    driver->owner == TIMER_DMA_OWNER_RECOVERY_REQUIRED &&
    driver->result == TIMER_DMA_RESULT_NONE
  ) {
    timer0_write_control(driver->timer, 0u);
    dma0_write_control(driver->dma, 0u);
    dma0_write_status_clear(driver->dma, DMA0_STATUS_ALL);
    timer0_write_period(driver->timer, driver->period_ticks);
    timer0_write_compare_shadow(driver->timer, driver->last_compare_ticks);
    timer0_write_control(driver->timer, TIMER0_CONTROL_READY);
    clear_active_sequence(driver);
    driver->error_flags = 0u;
    driver->owner = TIMER_DMA_OWNER_CPU;
    recovered = true;
  }
  timer_dma_irq_restore(irq_state);
  return recovered;
}

timer_dma_result_t timer_dma_take_result(
  timer_dma_t *driver,
  uint32_t *error_flags
) {
  timer_dma_result_t result;
  uint32_t irq_state;

  if (!timer_dma_is_ready(driver) || error_flags == NULL) {
    return TIMER_DMA_RESULT_NONE;
  }

  irq_state = timer_dma_irq_save_disable();
  result = driver->result;
  if (result == TIMER_DMA_RESULT_NONE) {
    *error_flags = 0u;
  } else {
    *error_flags = driver->error_flags;
    driver->error_flags = 0u;
    driver->result = TIMER_DMA_RESULT_NONE;
  }
  timer_dma_irq_restore(irq_state);
  return result;
}

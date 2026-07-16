#ifndef TIMER_DMA_HANDOFF_H
#define TIMER_DMA_HANDOFF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_timer_dma_handoff.h"

#define TIMER_DMA_MAX_SAMPLES DMA0_MAX_TRANSFER_COUNT

typedef enum {
  TIMER_DMA_OWNER_CPU = 0,
  TIMER_DMA_OWNER_DMA,
  TIMER_DMA_OWNER_ABORTING,
  TIMER_DMA_OWNER_RECOVERY_REQUIRED,
} timer_dma_owner_t;

typedef enum {
  TIMER_DMA_RESULT_NONE = 0,
  TIMER_DMA_RESULT_COMPLETE,
  TIMER_DMA_RESULT_ABORTED,
  TIMER_DMA_RESULT_ERROR,
} timer_dma_result_t;

typedef struct {
  volatile timer0_registers_t *timer;
  volatile dma0_registers_t *dma;
  const uint16_t *samples;
  size_t sample_count;
  uint16_t period_ticks;
  uint16_t last_compare_ticks;
  uint32_t error_flags;
  timer_dma_owner_t owner;
  timer_dma_result_t result;
  bool initialized;
} timer_dma_t;

bool timer_dma_init(
  timer_dma_t *driver,
  volatile timer0_registers_t *timer,
  volatile dma0_registers_t *dma,
  uint16_t period_ticks,
  uint16_t initial_compare_ticks
);
bool timer_dma_start(
  timer_dma_t *driver,
  const uint16_t *samples,
  size_t sample_count
);
bool timer_dma_abort(timer_dma_t *driver);
void timer_dma_irq(timer_dma_t *driver);
bool timer_dma_recover(timer_dma_t *driver);
timer_dma_result_t timer_dma_take_result(
  timer_dma_t *driver,
  uint32_t *error_flags
);

#endif

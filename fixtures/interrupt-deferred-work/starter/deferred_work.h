#ifndef DEFERRED_WORK_H
#define DEFERRED_WORK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>

#include "fixture_interrupt_work.h"

typedef struct {
  volatile interrupt_work_latch_t *latch;
  atomic_uint_fast32_t pending_sources;
  uint32_t enabled_sources;
  bool initialized;
} deferred_work_t;

bool deferred_work_init(
  deferred_work_t *dispatcher,
  volatile interrupt_work_latch_t *latch
);
bool deferred_work_configure_sources(
  deferred_work_t *dispatcher,
  uint32_t sources
);
void deferred_work_low_irq(deferred_work_t *dispatcher);
void deferred_work_high_irq(deferred_work_t *dispatcher);
uint32_t deferred_work_take(deferred_work_t *dispatcher);

#endif

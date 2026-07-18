#include "deferred_work.h"

static bool dispatcher_is_ready(const deferred_work_t *dispatcher) {
  return dispatcher != NULL && dispatcher->initialized &&
    dispatcher->latch != NULL;
}

static void deferred_work_handle_irq(
  deferred_work_t *dispatcher,
  uint32_t source
) {
  if (!dispatcher_is_ready(dispatcher)) return;

  const uint32_t observed = interrupt_work_read_status(dispatcher->latch) &
    source;
  if (observed == 0u) return;

  interrupt_work_write_status_clear(dispatcher->latch, observed);
  (void) atomic_fetch_or_explicit(
    &dispatcher->pending_sources,
    (uint_fast32_t) observed,
    memory_order_release
  );
}

bool deferred_work_init(
  deferred_work_t *dispatcher,
  volatile interrupt_work_latch_t *latch
) {
  if (dispatcher == NULL || latch == NULL) return false;

  interrupt_work_write_enable(latch, 0u);
  interrupt_work_write_status_clear(latch, INTERRUPT_WORK_SOURCE_MASK);
  dispatcher->latch = latch;
  atomic_init(&dispatcher->pending_sources, 0u);
  dispatcher->enabled_sources = 0u;
  dispatcher->initialized = true;
  return true;
}

bool deferred_work_configure_sources(
  deferred_work_t *dispatcher,
  uint32_t sources
) {
  if (
    !dispatcher_is_ready(dispatcher) ||
    (sources & ~INTERRUPT_WORK_SOURCE_MASK) != 0u
  ) {
    return false;
  }

  const uint32_t interrupt_state = interrupt_work_irq_save_disable();
  interrupt_work_write_enable(dispatcher->latch, 0u);
  interrupt_work_write_status_clear(
    dispatcher->latch,
    INTERRUPT_WORK_SOURCE_MASK
  );
  interrupt_work_write_enable(dispatcher->latch, sources);
  dispatcher->enabled_sources = sources;
  interrupt_work_irq_restore(interrupt_state);
  return true;
}

void deferred_work_low_irq(deferred_work_t *dispatcher) {
  deferred_work_handle_irq(dispatcher, INTERRUPT_WORK_SOURCE_LOW);
}

void deferred_work_high_irq(deferred_work_t *dispatcher) {
  deferred_work_handle_irq(dispatcher, INTERRUPT_WORK_SOURCE_HIGH);
}

uint32_t deferred_work_take(deferred_work_t *dispatcher) {
  if (!dispatcher_is_ready(dispatcher)) return 0u;

  return (uint32_t) atomic_exchange_explicit(
    &dispatcher->pending_sources,
    0u,
    memory_order_acq_rel
  );
}

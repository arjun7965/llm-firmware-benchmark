#ifndef FIXTURE_INTERRUPT_WORK_H
#define FIXTURE_INTERRUPT_WORK_H

#include <stdint.h>

#define INTERRUPT_WORK_SOURCE_LOW UINT32_C(1)
#define INTERRUPT_WORK_SOURCE_HIGH (UINT32_C(1) << 1)
#define INTERRUPT_WORK_SOURCE_MASK \
  (INTERRUPT_WORK_SOURCE_LOW | INTERRUPT_WORK_SOURCE_HIGH)

typedef struct interrupt_work_latch interrupt_work_latch_t;

uint32_t interrupt_work_read_status(
  const volatile interrupt_work_latch_t *latch
);
void interrupt_work_write_status_clear(
  volatile interrupt_work_latch_t *latch,
  uint32_t value
);
void interrupt_work_write_enable(
  volatile interrupt_work_latch_t *latch,
  uint32_t value
);

uint32_t interrupt_work_irq_save_disable(void);
void interrupt_work_irq_restore(uint32_t state);

#endif

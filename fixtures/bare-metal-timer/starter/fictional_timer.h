#ifndef FICTIONAL_TIMER_H
#define FICTIONAL_TIMER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TIMER0_BASE_ADDRESS UINT32_C(0x40010000)
#define TIMER0_TICK_HZ UINT32_C(1000000)

#define TIMER0_CONTROL_ENABLE UINT32_C(1)
#define TIMER0_CONTROL_PERIODIC (UINT32_C(1) << 1)
#define TIMER0_CONTROL_IRQ_ENABLE (UINT32_C(1) << 2)
#define TIMER0_STATUS_IRQ_PENDING UINT32_C(1)

#define TIMER0_PRESCALER_MAX UINT32_C(0xFF)
#define TIMER0_RELOAD_MAX UINT32_C(0x00FFFFFF)

#define TIMER0_CONTROL_OFFSET UINT32_C(0x00)
#define TIMER0_PRESCALER_OFFSET UINT32_C(0x04)
#define TIMER0_RELOAD_OFFSET UINT32_C(0x08)
#define TIMER0_COUNT_OFFSET UINT32_C(0x0C)
#define TIMER0_STATUS_OFFSET UINT32_C(0x10)
#define TIMER0_IRQ_CLEAR_OFFSET UINT32_C(0x14)

typedef struct {
  volatile uint32_t control;
  volatile uint32_t prescaler;
  volatile uint32_t reload;
  volatile uint32_t count;
  volatile uint32_t status;
  volatile uint32_t irq_clear;
} timer0_registers_t;

_Static_assert(
  offsetof(timer0_registers_t, control) == TIMER0_CONTROL_OFFSET,
  "unexpected TIMER0 control offset"
);
_Static_assert(
  offsetof(timer0_registers_t, prescaler) == TIMER0_PRESCALER_OFFSET,
  "unexpected TIMER0 prescaler offset"
);
_Static_assert(
  offsetof(timer0_registers_t, reload) == TIMER0_RELOAD_OFFSET,
  "unexpected TIMER0 reload offset"
);
_Static_assert(
  offsetof(timer0_registers_t, count) == TIMER0_COUNT_OFFSET,
  "unexpected TIMER0 count offset"
);
_Static_assert(
  offsetof(timer0_registers_t, status) == TIMER0_STATUS_OFFSET,
  "unexpected TIMER0 status offset"
);
_Static_assert(
  offsetof(timer0_registers_t, irq_clear) == TIMER0_IRQ_CLEAR_OFFSET,
  "unexpected TIMER0 interrupt-clear offset"
);
_Static_assert(sizeof(timer0_registers_t) == 24u, "unexpected TIMER0 size");

#if defined(TIMER0_HOST_TEST)
uint32_t timer0_host_read(
  const volatile timer0_registers_t *timer,
  uint32_t offset
);
void timer0_host_write(
  volatile timer0_registers_t *timer,
  uint32_t offset,
  uint32_t value
);
#endif

static inline uint32_t timer0_read_status(
  const volatile timer0_registers_t *timer
) {
#if defined(TIMER0_HOST_TEST)
  return timer0_host_read(timer, TIMER0_STATUS_OFFSET);
#else
  return timer->status;
#endif
}

static inline void timer0_write_control(
  volatile timer0_registers_t *timer,
  uint32_t value
) {
#if defined(TIMER0_HOST_TEST)
  timer0_host_write(timer, TIMER0_CONTROL_OFFSET, value);
#else
  timer->control = value;
#endif
}

static inline void timer0_write_prescaler(
  volatile timer0_registers_t *timer,
  uint32_t value
) {
#if defined(TIMER0_HOST_TEST)
  timer0_host_write(timer, TIMER0_PRESCALER_OFFSET, value);
#else
  timer->prescaler = value;
#endif
}

static inline void timer0_write_reload(
  volatile timer0_registers_t *timer,
  uint32_t value
) {
#if defined(TIMER0_HOST_TEST)
  timer0_host_write(timer, TIMER0_RELOAD_OFFSET, value);
#else
  timer->reload = value;
#endif
}

static inline void timer0_write_irq_clear(
  volatile timer0_registers_t *timer,
  uint32_t value
) {
#if defined(TIMER0_HOST_TEST)
  timer0_host_write(timer, TIMER0_IRQ_CLEAR_OFFSET, value);
#else
  timer->irq_clear = value;
#endif
}

bool timer0_configure_periodic(
  volatile timer0_registers_t *timer,
  uint32_t peripheral_clock_hz,
  uint32_t period_us
);
void timer0_stop(volatile timer0_registers_t *timer);
bool timer0_irq_pending(const volatile timer0_registers_t *timer);
void timer0_acknowledge_irq(volatile timer0_registers_t *timer);

#endif

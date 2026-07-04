#include "fictional_timer.h"

#include <stddef.h>

static void clear_pending_irq(volatile timer0_registers_t *timer) {
  timer0_write_irq_clear(timer, TIMER0_STATUS_IRQ_PENDING);
}

static void enable_periodic_timer(volatile timer0_registers_t *timer) {
  timer0_write_control(
    timer,
    TIMER0_CONTROL_ENABLE |
    TIMER0_CONTROL_PERIODIC |
    TIMER0_CONTROL_IRQ_ENABLE
  );
}

bool timer0_configure_periodic(
  volatile timer0_registers_t *timer,
  uint32_t peripheral_clock_hz,
  uint32_t period_us
) {
  if (
    timer == NULL ||
    peripheral_clock_hz == 0u ||
    peripheral_clock_hz % TIMER0_TICK_HZ != 0u ||
    period_us == 0u
  ) {
    return false;
  }

  const uint32_t divider = peripheral_clock_hz / TIMER0_TICK_HZ;
  if (
    divider > TIMER0_PRESCALER_MAX + 1u ||
    period_us > TIMER0_RELOAD_MAX + 1u
  ) {
    return false;
  }

  timer0_write_control(timer, 0u);
  timer0_write_prescaler(timer, divider - 1u);
  timer0_write_reload(timer, period_us - 1u);
  clear_pending_irq(timer);
  enable_periodic_timer(timer);
  return true;
}

void timer0_stop(volatile timer0_registers_t *timer) {
  if (timer != NULL) timer0_write_control(timer, 0u);
}

bool timer0_irq_pending(const volatile timer0_registers_t *timer) {
  return timer != NULL &&
    (timer0_read_status(timer) & TIMER0_STATUS_IRQ_PENDING) != 0u;
}

void timer0_acknowledge_irq(volatile timer0_registers_t *timer) {
  if (timer != NULL) clear_pending_irq(timer);
}

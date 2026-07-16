#ifndef FIXTURE_TIMER_CAPTURE_H
#define FIXTURE_TIMER_CAPTURE_H

#include <stdint.h>

#define TIMER1_BASE_ADDRESS UINT32_C(0x40013000)

#define TIMER1_COUNTER_MODULUS UINT32_C(65536)
#define TIMER1_COUNTER_HALF_RANGE UINT16_C(0x8000)
#define TIMER_CAPTURE_MAX_DELAY \
  (TIMER1_COUNTER_HALF_RANGE - UINT16_C(1))

#define TIMER1_CONTROL_ENABLE UINT32_C(1)
#define TIMER1_CONTROL_CAPTURE_IRQ_ENABLE (UINT32_C(1) << 1)
#define TIMER1_CONTROL_OVERFLOW_IRQ_ENABLE (UINT32_C(1) << 2)
#define TIMER1_CONTROL_COMPARE_IRQ_ENABLE (UINT32_C(1) << 3)
#define TIMER1_CONTROL_READY \
  (TIMER1_CONTROL_ENABLE | TIMER1_CONTROL_CAPTURE_IRQ_ENABLE | \
    TIMER1_CONTROL_OVERFLOW_IRQ_ENABLE)
#define TIMER1_CONTROL_COMPARE_ARMED \
  (TIMER1_CONTROL_READY | TIMER1_CONTROL_COMPARE_IRQ_ENABLE)
#define TIMER1_CONTROL_ALL TIMER1_CONTROL_COMPARE_ARMED

#define TIMER1_STATUS_CAPTURE UINT32_C(1)
#define TIMER1_STATUS_OVERFLOW (UINT32_C(1) << 1)
#define TIMER1_STATUS_COMPARE (UINT32_C(1) << 2)
#define TIMER1_STATUS_ALL \
  (TIMER1_STATUS_CAPTURE | TIMER1_STATUS_OVERFLOW | TIMER1_STATUS_COMPARE)

typedef struct timer1_registers timer1_registers_t;

uint16_t timer1_read_count(const volatile timer1_registers_t *timer);
uint16_t timer1_read_capture(const volatile timer1_registers_t *timer);
uint32_t timer1_read_status(const volatile timer1_registers_t *timer);
void timer1_write_control(volatile timer1_registers_t *timer, uint32_t value);
void timer1_write_compare(volatile timer1_registers_t *timer, uint16_t value);
void timer1_write_status_clear(
  volatile timer1_registers_t *timer,
  uint32_t value
);

uint32_t timer_capture_irq_save_disable(void);
void timer_capture_irq_restore(uint32_t state);

#endif

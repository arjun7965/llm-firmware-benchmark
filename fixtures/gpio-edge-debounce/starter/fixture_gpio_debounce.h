#ifndef FIXTURE_GPIO_DEBOUNCE_H
#define FIXTURE_GPIO_DEBOUNCE_H

#include <stdint.h>

#define GPIO0_BASE_ADDRESS UINT32_C(0x40020000)
#define GPIO0_BUTTON_PIN UINT32_C(5)
#define GPIO0_BUTTON_MASK (UINT32_C(1) << GPIO0_BUTTON_PIN)

typedef struct gpio0_registers gpio0_registers_t;

uint32_t gpio0_read_input(const volatile gpio0_registers_t *gpio);
uint32_t gpio0_read_edge_status(const volatile gpio0_registers_t *gpio);
void gpio0_write_rising_enable(
  volatile gpio0_registers_t *gpio,
  uint32_t value
);
void gpio0_write_falling_enable(
  volatile gpio0_registers_t *gpio,
  uint32_t value
);
void gpio0_write_irq_enable(
  volatile gpio0_registers_t *gpio,
  uint32_t value
);
void gpio0_write_wake_enable(
  volatile gpio0_registers_t *gpio,
  uint32_t value
);
void gpio0_write_edge_clear(
  volatile gpio0_registers_t *gpio,
  uint32_t value
);
void gpio0_write_wake_clear(
  volatile gpio0_registers_t *gpio,
  uint32_t value
);
uint32_t gpio0_irq_save_disable(void);
void gpio0_irq_restore(uint32_t state);

#endif

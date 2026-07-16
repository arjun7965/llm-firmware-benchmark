#ifndef GPIO_DEBOUNCE_H
#define GPIO_DEBOUNCE_H

#include <stdbool.h>
#include <stdint.h>

#include "fixture_gpio_debounce.h"

#define GPIO_DEBOUNCE_INTERVAL_MS UINT32_C(20)

typedef enum {
  GPIO_DEBOUNCE_EVENT_NONE = 0,
  GPIO_DEBOUNCE_EVENT_PRESSED,
  GPIO_DEBOUNCE_EVENT_RELEASED,
} gpio_debounce_event_t;

typedef struct {
  volatile gpio0_registers_t *gpio;
  bool stable_pressed;
  bool candidate_pressed;
  bool debounce_active;
  bool sleeping;
  uint32_t candidate_started_at_ms;
  bool initialized;
} gpio_debounce_t;

bool gpio_debounce_init(
  gpio_debounce_t *driver,
  volatile gpio0_registers_t *gpio
);
void gpio_debounce_irq(gpio_debounce_t *driver, uint32_t now_ms);
gpio_debounce_event_t gpio_debounce_poll(
  gpio_debounce_t *driver,
  uint32_t now_ms
);
bool gpio_debounce_prepare_sleep(gpio_debounce_t *driver);
bool gpio_debounce_resume(gpio_debounce_t *driver, uint32_t now_ms);
bool gpio_debounce_is_pressed(gpio_debounce_t *driver);

#endif

#ifndef MOCK_GPIO_DEBOUNCE_H
#define MOCK_GPIO_DEBOUNCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_gpio_debounce.h"

typedef enum {
  MOCK_GPIO_EVENT_INPUT_READ,
  MOCK_GPIO_EVENT_EDGE_STATUS_READ,
  MOCK_GPIO_EVENT_RISING_ENABLE_WRITE,
  MOCK_GPIO_EVENT_FALLING_ENABLE_WRITE,
  MOCK_GPIO_EVENT_IRQ_ENABLE_WRITE,
  MOCK_GPIO_EVENT_WAKE_ENABLE_WRITE,
  MOCK_GPIO_EVENT_EDGE_CLEAR_WRITE,
  MOCK_GPIO_EVENT_WAKE_CLEAR_WRITE,
  MOCK_GPIO_EVENT_IRQ_SAVE_DISABLE,
  MOCK_GPIO_EVENT_IRQ_RESTORE,
  MOCK_GPIO_EVENT_COUNT,
} mock_gpio_event_t;

void mock_gpio_reset(void);
volatile gpio0_registers_t *mock_gpio0(void);
void mock_gpio_set_button_pressed(bool pressed);
void mock_gpio_set_edge_status(uint32_t value);
void mock_gpio_set_wake_status(uint32_t value);
void mock_gpio_set_irq_state(uint32_t value);

uint32_t mock_gpio_input(void);
uint32_t mock_gpio_edge_status(void);
uint32_t mock_gpio_wake_status(void);
uint32_t mock_gpio_rising_enable(void);
uint32_t mock_gpio_falling_enable(void);
uint32_t mock_gpio_irq_enable(void);
uint32_t mock_gpio_wake_enable(void);
uint32_t mock_gpio_irq_state(void);
size_t mock_gpio_event_count(void);
mock_gpio_event_t mock_gpio_event_at(size_t index);
uint32_t mock_gpio_event_value(size_t index);
bool mock_gpio_invalid_access(void);

#endif

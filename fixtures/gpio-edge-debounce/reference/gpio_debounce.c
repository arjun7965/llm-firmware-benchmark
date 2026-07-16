#include <stddef.h>

#include "gpio_debounce.h"

static bool button_is_pressed(const volatile gpio0_registers_t *gpio) {
  return (gpio0_read_input(gpio) & GPIO0_BUTTON_MASK) == 0u;
}

static void arm_normal_irq(gpio_debounce_t *driver) {
  gpio0_write_irq_enable(driver->gpio, GPIO0_BUTTON_MASK);
}

bool gpio_debounce_init(
  gpio_debounce_t *driver,
  volatile gpio0_registers_t *gpio
) {
  bool stable_pressed;

  if (driver == NULL || gpio == NULL) return false;

  gpio0_write_irq_enable(gpio, 0u);
  gpio0_write_wake_enable(gpio, 0u);
  gpio0_write_rising_enable(gpio, GPIO0_BUTTON_MASK);
  gpio0_write_falling_enable(gpio, GPIO0_BUTTON_MASK);
  gpio0_write_edge_clear(gpio, GPIO0_BUTTON_MASK);
  gpio0_write_wake_clear(gpio, GPIO0_BUTTON_MASK);
  stable_pressed = button_is_pressed(gpio);
  gpio0_write_irq_enable(gpio, GPIO0_BUTTON_MASK);

  *driver = (gpio_debounce_t) {
    .gpio = gpio,
    .stable_pressed = stable_pressed,
    .candidate_pressed = stable_pressed,
    .debounce_active = false,
    .sleeping = false,
    .candidate_started_at_ms = 0u,
    .initialized = true,
  };
  return true;
}

void gpio_debounce_irq(gpio_debounce_t *driver, uint32_t now_ms) {
  uint32_t status;

  if (driver == NULL || !driver->initialized || driver->sleeping) return;

  status = gpio0_read_edge_status(driver->gpio);
  if ((status & GPIO0_BUTTON_MASK) == 0u) return;

  gpio0_write_irq_enable(driver->gpio, 0u);
  gpio0_write_edge_clear(driver->gpio, GPIO0_BUTTON_MASK);
  if (driver->debounce_active) return;

  driver->candidate_pressed = button_is_pressed(driver->gpio);
  driver->candidate_started_at_ms = now_ms;
  driver->debounce_active = true;
}

gpio_debounce_event_t gpio_debounce_poll(
  gpio_debounce_t *driver,
  uint32_t now_ms
) {
  gpio_debounce_event_t event = GPIO_DEBOUNCE_EVENT_NONE;
  uint32_t irq_state;
  bool observed_pressed;

  if (driver == NULL || !driver->initialized) return GPIO_DEBOUNCE_EVENT_NONE;

  irq_state = gpio0_irq_save_disable();
  if (driver->sleeping || !driver->debounce_active ||
      (uint32_t)(now_ms - driver->candidate_started_at_ms) <
        GPIO_DEBOUNCE_INTERVAL_MS) {
    gpio0_irq_restore(irq_state);
    return GPIO_DEBOUNCE_EVENT_NONE;
  }

  observed_pressed = button_is_pressed(driver->gpio);
  if (observed_pressed != driver->candidate_pressed) {
    driver->candidate_pressed = observed_pressed;
    driver->candidate_started_at_ms = now_ms;
    gpio0_irq_restore(irq_state);
    return GPIO_DEBOUNCE_EVENT_NONE;
  }

  driver->debounce_active = false;
  if (observed_pressed != driver->stable_pressed) {
    driver->stable_pressed = observed_pressed;
    event = observed_pressed
      ? GPIO_DEBOUNCE_EVENT_PRESSED
      : GPIO_DEBOUNCE_EVENT_RELEASED;
  }
  arm_normal_irq(driver);
  gpio0_irq_restore(irq_state);
  return event;
}

bool gpio_debounce_prepare_sleep(gpio_debounce_t *driver) {
  uint32_t irq_state;

  if (driver == NULL || !driver->initialized || driver->sleeping) return false;

  irq_state = gpio0_irq_save_disable();
  if (driver->debounce_active) {
    gpio0_irq_restore(irq_state);
    return false;
  }

  gpio0_write_irq_enable(driver->gpio, 0u);
  gpio0_write_edge_clear(driver->gpio, GPIO0_BUTTON_MASK);
  gpio0_write_wake_clear(driver->gpio, GPIO0_BUTTON_MASK);
  gpio0_write_wake_enable(driver->gpio, GPIO0_BUTTON_MASK);
  driver->sleeping = true;
  gpio0_irq_restore(irq_state);
  return true;
}

bool gpio_debounce_resume(gpio_debounce_t *driver, uint32_t now_ms) {
  uint32_t irq_state;
  bool observed_pressed;

  if (driver == NULL || !driver->initialized || !driver->sleeping) {
    return false;
  }

  irq_state = gpio0_irq_save_disable();
  gpio0_write_wake_enable(driver->gpio, 0u);
  gpio0_write_edge_clear(driver->gpio, GPIO0_BUTTON_MASK);
  gpio0_write_wake_clear(driver->gpio, GPIO0_BUTTON_MASK);
  observed_pressed = button_is_pressed(driver->gpio);
  driver->sleeping = false;

  if (observed_pressed != driver->stable_pressed) {
    driver->candidate_pressed = observed_pressed;
    driver->candidate_started_at_ms = now_ms;
    driver->debounce_active = true;
  } else {
    driver->debounce_active = false;
    arm_normal_irq(driver);
  }

  gpio0_irq_restore(irq_state);
  return true;
}

bool gpio_debounce_is_pressed(gpio_debounce_t *driver) {
  bool pressed;
  uint32_t irq_state;

  if (driver == NULL || !driver->initialized) return false;

  irq_state = gpio0_irq_save_disable();
  pressed = driver->stable_pressed;
  gpio0_irq_restore(irq_state);
  return pressed;
}

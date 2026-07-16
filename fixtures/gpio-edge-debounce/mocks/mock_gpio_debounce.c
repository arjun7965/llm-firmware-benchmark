#include "mock_gpio_debounce.h"

#define MOCK_GPIO_HISTORY_CAPACITY 192u

struct gpio0_registers {
  uint32_t reserved;
};

typedef struct {
  mock_gpio_event_t event;
  uint32_t value;
} mock_gpio_event_record_t;

typedef struct {
  struct gpio0_registers gpio;
  uint32_t input;
  uint32_t edge_status;
  uint32_t wake_status;
  uint32_t rising_enable;
  uint32_t falling_enable;
  uint32_t irq_enable;
  uint32_t wake_enable;
  uint32_t irq_state;
  mock_gpio_event_record_t events[MOCK_GPIO_HISTORY_CAPACITY];
  size_t event_count;
  bool invalid_access;
} mock_gpio_state_t;

static mock_gpio_state_t state;

static bool is_gpio(const volatile gpio0_registers_t *gpio) {
  return gpio == &state.gpio;
}

static void record_event(mock_gpio_event_t event, uint32_t value) {
  if (state.event_count < MOCK_GPIO_HISTORY_CAPACITY) {
    state.events[state.event_count] = (mock_gpio_event_record_t) {
      .event = event,
      .value = value,
    };
  } else {
    state.invalid_access = true;
  }
  state.event_count++;
}

static uint32_t button_masked(uint32_t value) {
  if ((value & ~GPIO0_BUTTON_MASK) != 0u) state.invalid_access = true;
  return value & GPIO0_BUTTON_MASK;
}

void mock_gpio_reset(void) {
  state = (mock_gpio_state_t) {
    .input = GPIO0_BUTTON_MASK,
    .irq_state = UINT32_C(1),
  };
}

volatile gpio0_registers_t *mock_gpio0(void) {
  return &state.gpio;
}

void mock_gpio_set_button_pressed(bool pressed) {
  const bool was_pressed = (state.input & GPIO0_BUTTON_MASK) == 0u;
  const bool rising = was_pressed && !pressed;
  const bool falling = !was_pressed && pressed;

  if (pressed) {
    state.input &= ~GPIO0_BUTTON_MASK;
  } else {
    state.input |= GPIO0_BUTTON_MASK;
  }
  if (!rising && !falling) return;

  if ((rising && (state.rising_enable & GPIO0_BUTTON_MASK) != 0u) ||
      (falling && (state.falling_enable & GPIO0_BUTTON_MASK) != 0u)) {
    state.edge_status |= GPIO0_BUTTON_MASK;
    if ((state.wake_enable & GPIO0_BUTTON_MASK) != 0u) {
      state.wake_status |= GPIO0_BUTTON_MASK;
    }
  }
}

void mock_gpio_set_edge_status(uint32_t value) {
  state.edge_status = value;
}

void mock_gpio_set_wake_status(uint32_t value) {
  state.wake_status = value;
}

void mock_gpio_set_irq_state(uint32_t value) {
  state.irq_state = value;
}

uint32_t mock_gpio_input(void) {
  return state.input;
}

uint32_t mock_gpio_edge_status(void) {
  return state.edge_status;
}

uint32_t mock_gpio_wake_status(void) {
  return state.wake_status;
}

uint32_t mock_gpio_rising_enable(void) {
  return state.rising_enable;
}

uint32_t mock_gpio_falling_enable(void) {
  return state.falling_enable;
}

uint32_t mock_gpio_irq_enable(void) {
  return state.irq_enable;
}

uint32_t mock_gpio_wake_enable(void) {
  return state.wake_enable;
}

uint32_t mock_gpio_irq_state(void) {
  return state.irq_state;
}

size_t mock_gpio_event_count(void) {
  return state.event_count;
}

mock_gpio_event_t mock_gpio_event_at(size_t index) {
  return index < state.event_count && index < MOCK_GPIO_HISTORY_CAPACITY
    ? state.events[index].event
    : MOCK_GPIO_EVENT_COUNT;
}

uint32_t mock_gpio_event_value(size_t index) {
  return index < state.event_count && index < MOCK_GPIO_HISTORY_CAPACITY
    ? state.events[index].value
    : UINT32_MAX;
}

bool mock_gpio_invalid_access(void) {
  return state.invalid_access;
}

uint32_t gpio0_read_input(const volatile gpio0_registers_t *gpio) {
  if (!is_gpio(gpio)) {
    state.invalid_access = true;
    return 0u;
  }
  record_event(MOCK_GPIO_EVENT_INPUT_READ, state.input);
  return state.input;
}

uint32_t gpio0_read_edge_status(const volatile gpio0_registers_t *gpio) {
  if (!is_gpio(gpio)) {
    state.invalid_access = true;
    return 0u;
  }
  record_event(MOCK_GPIO_EVENT_EDGE_STATUS_READ, state.edge_status);
  return state.edge_status;
}

void gpio0_write_rising_enable(
  volatile gpio0_registers_t *gpio,
  uint32_t value
) {
  if (!is_gpio(gpio)) {
    state.invalid_access = true;
    return;
  }
  state.rising_enable = button_masked(value);
  record_event(MOCK_GPIO_EVENT_RISING_ENABLE_WRITE, state.rising_enable);
}

void gpio0_write_falling_enable(
  volatile gpio0_registers_t *gpio,
  uint32_t value
) {
  if (!is_gpio(gpio)) {
    state.invalid_access = true;
    return;
  }
  state.falling_enable = button_masked(value);
  record_event(MOCK_GPIO_EVENT_FALLING_ENABLE_WRITE, state.falling_enable);
}

void gpio0_write_irq_enable(
  volatile gpio0_registers_t *gpio,
  uint32_t value
) {
  if (!is_gpio(gpio)) {
    state.invalid_access = true;
    return;
  }
  state.irq_enable = button_masked(value);
  record_event(MOCK_GPIO_EVENT_IRQ_ENABLE_WRITE, state.irq_enable);
}

void gpio0_write_wake_enable(
  volatile gpio0_registers_t *gpio,
  uint32_t value
) {
  if (!is_gpio(gpio)) {
    state.invalid_access = true;
    return;
  }
  state.wake_enable = button_masked(value);
  record_event(MOCK_GPIO_EVENT_WAKE_ENABLE_WRITE, state.wake_enable);
}

void gpio0_write_edge_clear(
  volatile gpio0_registers_t *gpio,
  uint32_t value
) {
  const uint32_t masked = button_masked(value);
  if (!is_gpio(gpio)) {
    state.invalid_access = true;
    return;
  }
  state.edge_status &= ~masked;
  record_event(MOCK_GPIO_EVENT_EDGE_CLEAR_WRITE, masked);
}

void gpio0_write_wake_clear(
  volatile gpio0_registers_t *gpio,
  uint32_t value
) {
  const uint32_t masked = button_masked(value);
  if (!is_gpio(gpio)) {
    state.invalid_access = true;
    return;
  }
  state.wake_status &= ~masked;
  record_event(MOCK_GPIO_EVENT_WAKE_CLEAR_WRITE, masked);
}

uint32_t gpio0_irq_save_disable(void) {
  const uint32_t previous_state = state.irq_state;
  state.irq_state = 0u;
  record_event(MOCK_GPIO_EVENT_IRQ_SAVE_DISABLE, previous_state);
  return previous_state;
}

void gpio0_irq_restore(uint32_t irq_state) {
  state.irq_state = irq_state;
  record_event(MOCK_GPIO_EVENT_IRQ_RESTORE, irq_state);
}

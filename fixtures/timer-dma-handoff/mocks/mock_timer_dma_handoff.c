#include "mock_timer_dma_handoff.h"

#define MOCK_TIMER_DMA_HISTORY_CAPACITY 192u

struct timer0_registers {
  uint32_t reserved;
};

struct dma0_registers {
  uint32_t reserved;
};

typedef struct {
  mock_timer_dma_event_t event;
  uint32_t value;
} mock_timer_dma_event_record_t;

typedef struct {
  struct timer0_registers timer;
  struct dma0_registers dma;
  uint32_t timer_control;
  uint16_t timer_period;
  uint16_t timer_compare_shadow;
  uint16_t timer_compare_active;
  uintptr_t dma_source;
  uintptr_t dma_destination;
  uint32_t dma_count;
  uint32_t dma_transferred;
  uint32_t dma_control;
  uint32_t dma_status;
  uint32_t irq_state;
  mock_timer_dma_event_record_t events[MOCK_TIMER_DMA_HISTORY_CAPACITY];
  size_t event_count;
  bool invalid_access;
} mock_timer_dma_state_t;

static mock_timer_dma_state_t state;

static bool is_timer(const volatile timer0_registers_t *timer) {
  return timer == &state.timer;
}

static bool is_dma(const volatile dma0_registers_t *dma) {
  return dma == &state.dma;
}

static void record_event(mock_timer_dma_event_t event, uint32_t value) {
  if (state.event_count < MOCK_TIMER_DMA_HISTORY_CAPACITY) {
    state.events[state.event_count] = (mock_timer_dma_event_record_t) {
      .event = event,
      .value = value,
    };
  } else {
    state.invalid_access = true;
  }
  state.event_count++;
}

static bool dma_descriptor_is_ready(void) {
  return state.dma_source != 0u &&
    state.dma_destination == (uintptr_t)&state.timer_compare_shadow &&
    state.dma_count > 0u && state.dma_count <= DMA0_MAX_TRANSFER_COUNT;
}

void mock_timer_dma_reset(void) {
  state = (mock_timer_dma_state_t) {
    .irq_state = UINT32_C(1),
  };
}

volatile timer0_registers_t *mock_timer0(void) {
  return &state.timer;
}

volatile dma0_registers_t *mock_dma0(void) {
  return &state.dma;
}

bool mock_timer_dma_tick(void) {
  const uint16_t *samples;
  uint16_t value;

  if (
    state.timer_control != TIMER0_CONTROL_DMA_OWNED ||
    state.dma_control != DMA0_CHANNEL_CONTROL_READY
  ) {
    return false;
  }
  if (!dma_descriptor_is_ready() || state.dma_transferred >= state.dma_count) {
    state.invalid_access = true;
    state.dma_status |= DMA0_STATUS_ERROR;
    state.dma_control = 0u;
    return false;
  }

  samples = (const uint16_t *)state.dma_source;
  value = samples[state.dma_transferred];
  if (value > state.timer_period) {
    state.invalid_access = true;
    state.dma_status |= DMA0_STATUS_ERROR;
    state.dma_control = 0u;
    return false;
  }
  state.timer_compare_shadow = value;
  state.timer_compare_active = value;
  state.dma_transferred++;
  if (state.dma_transferred == state.dma_count) {
    state.dma_status |= DMA0_STATUS_COMPLETE;
    state.dma_control = 0u;
  }
  return true;
}

void mock_timer_dma_set_status(uint32_t value) {
  state.dma_status = value & DMA0_STATUS_ALL;
}

void mock_timer_dma_set_irq_state(uint32_t value) {
  state.irq_state = value;
}

uint32_t mock_timer0_control(void) {
  return state.timer_control;
}

uint16_t mock_timer0_period(void) {
  return state.timer_period;
}

uint16_t mock_timer0_compare_shadow(void) {
  return state.timer_compare_shadow;
}

uint16_t mock_timer0_compare_active(void) {
  return state.timer_compare_active;
}

uint32_t mock_dma0_control(void) {
  return state.dma_control;
}

uintptr_t mock_dma0_source(void) {
  return state.dma_source;
}

uintptr_t mock_dma0_destination(void) {
  return state.dma_destination;
}

uint32_t mock_dma0_count(void) {
  return state.dma_count;
}

uint32_t mock_dma0_transferred(void) {
  return state.dma_transferred;
}

uint32_t mock_dma0_status(void) {
  return state.dma_status;
}

uint32_t mock_timer_dma_irq_state(void) {
  return state.irq_state;
}

size_t mock_timer_dma_event_count(void) {
  return state.event_count;
}

mock_timer_dma_event_t mock_timer_dma_event_at(size_t index) {
  return index < state.event_count && index < MOCK_TIMER_DMA_HISTORY_CAPACITY
    ? state.events[index].event
    : MOCK_TIMER_DMA_EVENT_COUNT;
}

uint32_t mock_timer_dma_event_value(size_t index) {
  return index < state.event_count && index < MOCK_TIMER_DMA_HISTORY_CAPACITY
    ? state.events[index].value
    : UINT32_MAX;
}

bool mock_timer_dma_invalid_access(void) {
  return state.invalid_access;
}

uintptr_t timer_dma_buffer_address(const void *buffer) {
  return (uintptr_t)buffer;
}

uintptr_t timer0_compare_dma_address(const volatile timer0_registers_t *timer) {
  if (!is_timer(timer)) {
    state.invalid_access = true;
    return 0u;
  }
  return (uintptr_t)&state.timer_compare_shadow;
}

void timer0_write_control(
  volatile timer0_registers_t *timer,
  uint32_t value
) {
  if (!is_timer(timer)) {
    state.invalid_access = true;
    return;
  }
  if ((value & ~TIMER0_CONTROL_ALL) != 0u) {
    state.invalid_access = true;
  }
  if (
    value == TIMER0_CONTROL_DMA_OWNED &&
    state.dma_control != DMA0_CHANNEL_CONTROL_READY
  ) {
    state.invalid_access = true;
  }
  state.timer_control = value & TIMER0_CONTROL_ALL;
  record_event(MOCK_TIMER_DMA_EVENT_TIMER_CONTROL_WRITE, state.timer_control);
}

void timer0_write_period(volatile timer0_registers_t *timer, uint16_t value) {
  if (!is_timer(timer)) {
    state.invalid_access = true;
    return;
  }
  if (
    state.timer_control != 0u || value < UINT16_C(2) ||
    value > TIMER0_MAX_PERIOD_TICKS
  ) {
    state.invalid_access = true;
  }
  state.timer_period = value;
  record_event(MOCK_TIMER_DMA_EVENT_TIMER_PERIOD_WRITE, value);
}

void timer0_write_compare_shadow(
  volatile timer0_registers_t *timer,
  uint16_t value
) {
  if (!is_timer(timer)) {
    state.invalid_access = true;
    return;
  }
  if (state.timer_control != 0u || value > state.timer_period) {
    state.invalid_access = true;
  }
  state.timer_compare_shadow = value;
  state.timer_compare_active = value;
  record_event(MOCK_TIMER_DMA_EVENT_TIMER_COMPARE_SHADOW_WRITE, value);
}

uint16_t timer0_read_compare_active(
  const volatile timer0_registers_t *timer
) {
  if (!is_timer(timer)) {
    state.invalid_access = true;
    return 0u;
  }
  record_event(
    MOCK_TIMER_DMA_EVENT_TIMER_COMPARE_ACTIVE_READ,
    state.timer_compare_active
  );
  return state.timer_compare_active;
}

void dma0_write_source(volatile dma0_registers_t *dma, uintptr_t value) {
  if (!is_dma(dma)) {
    state.invalid_access = true;
    return;
  }
  if (state.dma_control != 0u) state.invalid_access = true;
  state.dma_source = value;
  record_event(MOCK_TIMER_DMA_EVENT_DMA_SOURCE_WRITE, 0u);
}

void dma0_write_destination(volatile dma0_registers_t *dma, uintptr_t value) {
  if (!is_dma(dma)) {
    state.invalid_access = true;
    return;
  }
  if (state.dma_control != 0u) state.invalid_access = true;
  state.dma_destination = value;
  record_event(MOCK_TIMER_DMA_EVENT_DMA_DESTINATION_WRITE, 0u);
}

void dma0_write_count(volatile dma0_registers_t *dma, uint32_t value) {
  if (!is_dma(dma)) {
    state.invalid_access = true;
    return;
  }
  if (
    state.dma_control != 0u || value == 0u ||
    value > DMA0_MAX_TRANSFER_COUNT
  ) {
    state.invalid_access = true;
  }
  state.dma_count = value;
  record_event(MOCK_TIMER_DMA_EVENT_DMA_COUNT_WRITE, value);
}

void dma0_write_control(volatile dma0_registers_t *dma, uint32_t value) {
  if (!is_dma(dma)) {
    state.invalid_access = true;
    return;
  }
  if ((value & ~DMA0_CHANNEL_CONTROL_ALL) != 0u) {
    state.invalid_access = true;
  }
  if (value == DMA0_CHANNEL_CONTROL_ABORT) {
    if (
      state.dma_control != DMA0_CHANNEL_CONTROL_READY ||
      state.timer_control != TIMER0_CONTROL_READY
    ) {
      state.invalid_access = true;
    } else {
      state.dma_control = 0u;
      state.dma_status |= DMA0_STATUS_ABORTED;
    }
  } else if (value == 0u) {
    state.dma_control = 0u;
  } else if (value != DMA0_CHANNEL_CONTROL_READY || !dma_descriptor_is_ready()) {
    state.invalid_access = true;
    state.dma_control = value & DMA0_CHANNEL_CONTROL_ALL;
  } else {
    state.dma_control = value;
    state.dma_transferred = 0u;
  }
  record_event(
    MOCK_TIMER_DMA_EVENT_DMA_CONTROL_WRITE,
    value & DMA0_CHANNEL_CONTROL_ALL
  );
}

uint32_t dma0_read_status(const volatile dma0_registers_t *dma) {
  if (!is_dma(dma)) {
    state.invalid_access = true;
    return 0u;
  }
  record_event(MOCK_TIMER_DMA_EVENT_DMA_STATUS_READ, state.dma_status);
  return state.dma_status;
}

void dma0_write_status_clear(volatile dma0_registers_t *dma, uint32_t value) {
  if (!is_dma(dma)) {
    state.invalid_access = true;
    return;
  }
  if ((value & ~DMA0_STATUS_ALL) != 0u) state.invalid_access = true;
  state.dma_status &= ~(value & DMA0_STATUS_ALL);
  record_event(
    MOCK_TIMER_DMA_EVENT_DMA_STATUS_CLEAR_WRITE,
    value & DMA0_STATUS_ALL
  );
}

uint32_t timer_dma_irq_save_disable(void) {
  const uint32_t previous_state = state.irq_state;

  state.irq_state = 0u;
  record_event(MOCK_TIMER_DMA_EVENT_IRQ_SAVE_DISABLE, previous_state);
  return previous_state;
}

void timer_dma_irq_restore(uint32_t irq_state) {
  state.irq_state = irq_state;
  record_event(MOCK_TIMER_DMA_EVENT_IRQ_RESTORE, irq_state);
}

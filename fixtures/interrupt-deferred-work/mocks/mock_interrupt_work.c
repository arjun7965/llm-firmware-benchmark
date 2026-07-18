#include "mock_interrupt_work.h"

#define MOCK_INTERRUPT_WORK_HISTORY_CAPACITY 96u

struct interrupt_work_latch {
  uint32_t reserved;
};

typedef struct {
  mock_interrupt_work_event_t event;
  uint32_t value;
} mock_interrupt_work_event_record_t;

typedef struct {
  struct interrupt_work_latch latch;
  uint32_t status;
  uint32_t enable;
  uint32_t irq_state;
  mock_interrupt_work_event_record_t
    events[MOCK_INTERRUPT_WORK_HISTORY_CAPACITY];
  size_t event_count;
  size_t irq_save_count;
  size_t irq_restore_count;
  uint32_t last_irq_restore;
  mock_interrupt_work_hook_t status_read_hook;
  void *status_read_hook_context;
  bool invalid_access;
} mock_interrupt_work_state_t;

static mock_interrupt_work_state_t state;

static bool is_latch(const volatile interrupt_work_latch_t *latch) {
  return latch == &state.latch;
}

static void record_event(mock_interrupt_work_event_t event, uint32_t value) {
  if (state.event_count < MOCK_INTERRUPT_WORK_HISTORY_CAPACITY) {
    state.events[state.event_count] = (mock_interrupt_work_event_record_t) {
      .event = event,
      .value = value,
    };
  } else {
    state.invalid_access = true;
  }
  state.event_count++;
}

void mock_interrupt_work_reset(void) {
  state = (mock_interrupt_work_state_t) {
    .irq_state = UINT32_C(0xA5A55A5A),
  };
}

volatile interrupt_work_latch_t *mock_interrupt_work_latch(void) {
  return &state.latch;
}

void mock_interrupt_work_raise(uint32_t sources) {
  if ((sources & ~INTERRUPT_WORK_SOURCE_MASK) != 0u) {
    state.invalid_access = true;
    return;
  }
  state.status |= sources;
}

void mock_interrupt_work_set_next_status_read_hook(
  mock_interrupt_work_hook_t hook,
  void *context
) {
  state.status_read_hook = hook;
  state.status_read_hook_context = context;
}

void mock_interrupt_work_clear_events(void) {
  state.event_count = 0u;
}

size_t mock_interrupt_work_event_count(void) {
  return state.event_count;
}

mock_interrupt_work_event_t mock_interrupt_work_event_at(size_t index) {
  return index < state.event_count &&
      index < MOCK_INTERRUPT_WORK_HISTORY_CAPACITY
    ? state.events[index].event
    : MOCK_INTERRUPT_WORK_EVENT_COUNT;
}

uint32_t mock_interrupt_work_event_value(size_t index) {
  return index < state.event_count &&
      index < MOCK_INTERRUPT_WORK_HISTORY_CAPACITY
    ? state.events[index].value
    : UINT32_MAX;
}

uint32_t mock_interrupt_work_status(void) {
  return state.status;
}

uint32_t mock_interrupt_work_enable(void) {
  return state.enable;
}

void mock_interrupt_work_set_irq_state(uint32_t irq_state) {
  state.irq_state = irq_state;
}

uint32_t mock_interrupt_work_irq_state(void) {
  return state.irq_state;
}

size_t mock_interrupt_work_irq_save_count(void) {
  return state.irq_save_count;
}

size_t mock_interrupt_work_irq_restore_count(void) {
  return state.irq_restore_count;
}

uint32_t mock_interrupt_work_last_irq_restore(void) {
  return state.last_irq_restore;
}

bool mock_interrupt_work_invalid_access(void) {
  return state.invalid_access;
}

uint32_t interrupt_work_read_status(
  const volatile interrupt_work_latch_t *latch
) {
  if (!is_latch(latch)) {
    state.invalid_access = true;
    return 0u;
  }

  const uint32_t observed = state.status;
  record_event(MOCK_INTERRUPT_WORK_EVENT_STATUS_READ, observed);
  if (state.status_read_hook != NULL) {
    const mock_interrupt_work_hook_t hook = state.status_read_hook;
    void *const context = state.status_read_hook_context;
    state.status_read_hook = NULL;
    state.status_read_hook_context = NULL;
    hook(context);
  }
  return observed;
}

void interrupt_work_write_status_clear(
  volatile interrupt_work_latch_t *latch,
  uint32_t value
) {
  if (!is_latch(latch) ||
      (value & ~INTERRUPT_WORK_SOURCE_MASK) != 0u) {
    state.invalid_access = true;
    return;
  }

  state.status &= ~value;
  record_event(MOCK_INTERRUPT_WORK_EVENT_STATUS_CLEAR, value);
}

void interrupt_work_write_enable(
  volatile interrupt_work_latch_t *latch,
  uint32_t value
) {
  if (!is_latch(latch) ||
      (value & ~INTERRUPT_WORK_SOURCE_MASK) != 0u) {
    state.invalid_access = true;
    return;
  }

  state.enable = value;
  record_event(MOCK_INTERRUPT_WORK_EVENT_ENABLE_WRITE, value);
}

uint32_t interrupt_work_irq_save_disable(void) {
  const uint32_t saved_state = state.irq_state;
  state.irq_state = 0u;
  state.irq_save_count++;
  record_event(MOCK_INTERRUPT_WORK_EVENT_IRQ_SAVE, saved_state);
  return saved_state;
}

void interrupt_work_irq_restore(uint32_t irq_state) {
  state.irq_state = irq_state;
  state.last_irq_restore = irq_state;
  state.irq_restore_count++;
  record_event(MOCK_INTERRUPT_WORK_EVENT_IRQ_RESTORE, irq_state);
}

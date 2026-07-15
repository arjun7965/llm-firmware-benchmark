#include "mock_vector_table.h"

#define MOCK_VECTOR_HISTORY_CAPACITY 96u

struct system_control_block {
  uintptr_t vtor;
};

struct nvic {
  uint32_t enabled;
  uint32_t pending;
};

typedef struct {
  mock_vector_event_t event;
  size_t index;
  uintptr_t value;
} mock_vector_event_record_t;

typedef struct {
  struct system_control_block scb;
  struct nvic nvic;
  mock_vector_event_record_t events[MOCK_VECTOR_HISTORY_CAPACITY];
  size_t event_count;
  bool interrupts_enabled;
  size_t irq_save_count;
  size_t irq_restore_count;
  uint32_t last_irq_restore;
  bool invalid_access;
  bool table_address_overridden;
  uintptr_t table_address_override;
} mock_vector_state_t;

static mock_vector_state_t state;

static bool is_scb(const volatile system_control_block_t *scb) {
  return scb == &state.scb;
}

static bool is_nvic(const volatile nvic_t *nvic) {
  return nvic == &state.nvic;
}

static void record_event(
  mock_vector_event_t event,
  size_t index,
  uintptr_t value
) {
  if (state.event_count < MOCK_VECTOR_HISTORY_CAPACITY) {
    state.events[state.event_count] = (mock_vector_event_record_t) {
      .event = event,
      .index = index,
      .value = value,
    };
  } else {
    state.invalid_access = true;
  }
  state.event_count++;
}

void mock_vector_reset(void) {
  state = (mock_vector_state_t){ 0 };
  state.interrupts_enabled = true;
}

void mock_vector_clear_log(void) {
  state.event_count = 0u;
  state.irq_save_count = 0u;
  state.irq_restore_count = 0u;
  state.last_irq_restore = 0u;
  state.invalid_access = false;
}

volatile system_control_block_t *mock_vector_scb(void) {
  return &state.scb;
}

volatile nvic_t *mock_vector_nvic(void) {
  return &state.nvic;
}

void mock_vector_set_pending_irqs(uint32_t value) {
  state.nvic.pending = value & INTERRUPT_VECTOR_EXTERNAL_MASK;
}

void mock_vector_set_enabled_irqs(uint32_t value) {
  state.nvic.enabled = value & INTERRUPT_VECTOR_EXTERNAL_MASK;
}

uint32_t mock_vector_pending_irqs(void) {
  return state.nvic.pending;
}

uint32_t mock_vector_enabled_irqs(void) {
  return state.nvic.enabled;
}

uintptr_t mock_vector_vtor(void) {
  return state.scb.vtor;
}

void mock_vector_set_table_address_override(uintptr_t address) {
  state.table_address_overridden = true;
  state.table_address_override = address;
}

void mock_vector_clear_table_address_override(void) {
  state.table_address_overridden = false;
  state.table_address_override = 0u;
}

size_t mock_vector_event_count(void) {
  return state.event_count;
}

mock_vector_event_t mock_vector_event_at(size_t index) {
  return index < state.event_count && index < MOCK_VECTOR_HISTORY_CAPACITY
    ? state.events[index].event
    : MOCK_VECTOR_EVENT_COUNT;
}

size_t mock_vector_event_index(size_t index) {
  return index < state.event_count && index < MOCK_VECTOR_HISTORY_CAPACITY
    ? state.events[index].index
    : SIZE_MAX;
}

uintptr_t mock_vector_event_value(size_t index) {
  return index < state.event_count && index < MOCK_VECTOR_HISTORY_CAPACITY
    ? state.events[index].value
    : UINTPTR_MAX;
}

void mock_vector_set_interrupts_enabled(bool enabled) {
  state.interrupts_enabled = enabled;
}

bool mock_vector_interrupts_enabled(void) {
  return state.interrupts_enabled;
}

size_t mock_vector_irq_save_count(void) {
  return state.irq_save_count;
}

size_t mock_vector_irq_restore_count(void) {
  return state.irq_restore_count;
}

uint32_t mock_vector_last_irq_restore(void) {
  return state.last_irq_restore;
}

bool mock_vector_invalid_access(void) {
  return state.invalid_access;
}

uintptr_t interrupt_vector_table_address(
  const interrupt_vector_table_t *table
) {
  if (table == NULL) {
    state.invalid_access = true;
    return 0u;
  }
  return state.table_address_overridden
    ? state.table_address_override
    : (uintptr_t)table;
}

void interrupt_vector_write_entry(
  interrupt_vector_table_t *table,
  size_t index,
  uint32_t value
) {
  if (table == NULL || index >= INTERRUPT_VECTOR_ENTRY_COUNT) {
    state.invalid_access = true;
    return;
  }
  record_event(MOCK_VECTOR_EVENT_TABLE_WRITE, index, value);
  table->entries[index] = value;
}

void scb_write_vtor(
  volatile system_control_block_t *scb,
  uintptr_t value
) {
  if (!is_scb(scb)) {
    state.invalid_access = true;
    return;
  }
  record_event(MOCK_VECTOR_EVENT_SCB_VTOR, SIZE_MAX, value);
  state.scb.vtor = value;
}

void nvic_write_icer(volatile nvic_t *nvic, uint32_t value) {
  if (!is_nvic(nvic)) {
    state.invalid_access = true;
    return;
  }
  const uint32_t masked = value & INTERRUPT_VECTOR_EXTERNAL_MASK;
  record_event(MOCK_VECTOR_EVENT_NVIC_ICER, SIZE_MAX, masked);
  state.nvic.enabled &= ~masked;
}

void nvic_write_icpr(volatile nvic_t *nvic, uint32_t value) {
  if (!is_nvic(nvic)) {
    state.invalid_access = true;
    return;
  }
  const uint32_t masked = value & INTERRUPT_VECTOR_EXTERNAL_MASK;
  record_event(MOCK_VECTOR_EVENT_NVIC_ICPR, SIZE_MAX, masked);
  state.nvic.pending &= ~masked;
}

void nvic_write_iser(volatile nvic_t *nvic, uint32_t value) {
  if (!is_nvic(nvic)) {
    state.invalid_access = true;
    return;
  }
  const uint32_t masked = value & INTERRUPT_VECTOR_EXTERNAL_MASK;
  record_event(MOCK_VECTOR_EVENT_NVIC_ISER, SIZE_MAX, masked);
  state.nvic.enabled |= masked;
}

void interrupt_vector_sync_barrier(void) {
  record_event(MOCK_VECTOR_EVENT_BARRIER, SIZE_MAX, 0u);
}

uint32_t interrupt_vector_irq_save_disable(void) {
  const uint32_t previous = state.interrupts_enabled ? UINT32_C(1) : 0u;
  record_event(MOCK_VECTOR_EVENT_IRQ_SAVE, SIZE_MAX, previous);
  state.interrupts_enabled = false;
  state.irq_save_count++;
  return previous;
}

void interrupt_vector_irq_restore(uint32_t restore_state) {
  record_event(MOCK_VECTOR_EVENT_IRQ_RESTORE, SIZE_MAX, restore_state);
  state.last_irq_restore = restore_state;
  state.interrupts_enabled = restore_state != 0u;
  state.irq_restore_count++;
}

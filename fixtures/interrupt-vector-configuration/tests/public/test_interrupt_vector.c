#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "interrupt_vector.h"
#include "mock_vector_table.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
              __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

#define INITIAL_STACK_A UINT32_C(0x20002000)
#define INITIAL_STACK_B UINT32_C(0x20003000)
#define RESET_HANDLER_A UINT32_C(0x08000101)
#define RESET_HANDLER_B UINT32_C(0x08000501)
#define DEFAULT_HANDLER_A UINT32_C(0x08000301)
#define DEFAULT_HANDLER_B UINT32_C(0x08000701)

static void fill_table(interrupt_vector_table_t *table, uint32_t value) {
  for (size_t index = 0u; index < INTERRUPT_VECTOR_ENTRY_COUNT; index++) {
    table->entries[index] = value;
  }
}

static bool state_equals(
  const interrupt_vector_state_t *left,
  const interrupt_vector_state_t *right
) {
  return left->scb == right->scb && left->nvic == right->nvic &&
    left->table == right->table && left->initialized == right->initialized;
}

static bool table_equals_value(
  const interrupt_vector_table_t *table,
  uint32_t initial_stack_pointer,
  uint32_t reset_handler,
  uint32_t default_handler
) {
  if (table->entries[INTERRUPT_VECTOR_INITIAL_STACK_INDEX] !=
      initial_stack_pointer ||
    table->entries[INTERRUPT_VECTOR_RESET_INDEX] != reset_handler) {
    return false;
  }
  for (
    size_t index = INTERRUPT_VECTOR_FIRST_DEFAULT_INDEX;
    index < INTERRUPT_VECTOR_ENTRY_COUNT;
    index++
  ) {
    if (table->entries[index] != default_handler) return false;
  }
  return true;
}

static bool event_matches(
  size_t position,
  mock_vector_event_t event,
  size_t index,
  uintptr_t value
) {
  return mock_vector_event_at(position) == event &&
    mock_vector_event_index(position) == index &&
    mock_vector_event_value(position) == value;
}

static bool startup_events_match(
  uintptr_t table_address,
  uint32_t initial_stack_pointer,
  uint32_t reset_handler,
  uint32_t default_handler
) {
  const size_t table_event_start = 2u;
  const size_t first_barrier = table_event_start +
    INTERRUPT_VECTOR_ENTRY_COUNT;

  if (mock_vector_event_count() != first_barrier + 3u) return false;
  if (!event_matches(
    0u,
    MOCK_VECTOR_EVENT_NVIC_ICER,
    SIZE_MAX,
    INTERRUPT_VECTOR_EXTERNAL_MASK
  )) {
    return false;
  }
  if (!event_matches(
    1u,
    MOCK_VECTOR_EVENT_NVIC_ICPR,
    SIZE_MAX,
    INTERRUPT_VECTOR_EXTERNAL_MASK
  )) {
    return false;
  }
  for (size_t index = 0u; index < INTERRUPT_VECTOR_ENTRY_COUNT; index++) {
    const uint32_t expected = index == INTERRUPT_VECTOR_INITIAL_STACK_INDEX
      ? initial_stack_pointer
      : index == INTERRUPT_VECTOR_RESET_INDEX ? reset_handler : default_handler;
    if (!event_matches(
      table_event_start + index,
      MOCK_VECTOR_EVENT_TABLE_WRITE,
      index,
      expected
    )) {
      return false;
    }
  }
  return event_matches(
    first_barrier,
    MOCK_VECTOR_EVENT_BARRIER,
    SIZE_MAX,
    0u
  ) && event_matches(
    first_barrier + 1u,
    MOCK_VECTOR_EVENT_SCB_VTOR,
    SIZE_MAX,
    table_address
  ) && event_matches(
    first_barrier + 2u,
    MOCK_VECTOR_EVENT_BARRIER,
    SIZE_MAX,
    0u
  );
}

static bool initialize(
  interrupt_vector_state_t *state,
  interrupt_vector_table_t *table
) {
  return interrupt_vector_initialize(
    state,
    mock_vector_scb(),
    mock_vector_nvic(),
    table,
    INITIAL_STACK_A,
    RESET_HANDLER_A,
    DEFAULT_HANDLER_A
  );
}

static bool test_startup_sequence_and_layout(void) {
  interrupt_vector_table_t table;
  interrupt_vector_state_t state = {
    .scb = (volatile system_control_block_t *)(uintptr_t)UINT32_C(1),
    .nvic = (volatile nvic_t *)(uintptr_t)UINT32_C(1),
    .table = (interrupt_vector_table_t *)(uintptr_t)UINT32_C(1),
    .initialized = true,
  };

  fill_table(&table, UINT32_C(0xA5A5A5A5));
  mock_vector_reset();
  mock_vector_set_enabled_irqs(INTERRUPT_VECTOR_EXTERNAL_MASK);
  mock_vector_set_pending_irqs(INTERRUPT_VECTOR_EXTERNAL_MASK);

  CHECK(initialize(&state, &table));
  CHECK(state.scb == mock_vector_scb());
  CHECK(state.nvic == mock_vector_nvic());
  CHECK(state.table == &table);
  CHECK(state.initialized);
  CHECK(table_equals_value(
    &table,
    INITIAL_STACK_A,
    RESET_HANDLER_A,
    DEFAULT_HANDLER_A
  ));
  CHECK(mock_vector_vtor() == interrupt_vector_table_address(&table));
  CHECK(mock_vector_enabled_irqs() == 0u);
  CHECK(mock_vector_pending_irqs() == 0u);
  CHECK(startup_events_match(
    interrupt_vector_table_address(&table),
    INITIAL_STACK_A,
    RESET_HANDLER_A,
    DEFAULT_HANDLER_A
  ));
  CHECK(mock_vector_irq_save_count() == 0u);
  CHECK(mock_vector_irq_restore_count() == 0u);
  CHECK(!mock_vector_invalid_access());
  return true;
}

static bool test_invalid_initialization_has_no_side_effects(void) {
  interrupt_vector_table_t table;
  interrupt_vector_state_t state = {
    .scb = (volatile system_control_block_t *)(uintptr_t)UINT32_C(1),
    .nvic = (volatile nvic_t *)(uintptr_t)UINT32_C(2),
    .table = (interrupt_vector_table_t *)(uintptr_t)UINT32_C(3),
    .initialized = true,
  };
  const interrupt_vector_state_t before = state;
  uint32_t table_before[INTERRUPT_VECTOR_ENTRY_COUNT];

  fill_table(&table, UINT32_C(0x5A5A5A5A));
  for (size_t index = 0u; index < INTERRUPT_VECTOR_ENTRY_COUNT; index++) {
    table_before[index] = table.entries[index];
  }
  mock_vector_reset();

  CHECK(!interrupt_vector_initialize(
    NULL,
    mock_vector_scb(),
    mock_vector_nvic(),
    &table,
    INITIAL_STACK_A,
    RESET_HANDLER_A,
    DEFAULT_HANDLER_A
  ));
  CHECK(!interrupt_vector_initialize(
    &state,
    NULL,
    mock_vector_nvic(),
    &table,
    INITIAL_STACK_A,
    RESET_HANDLER_A,
    DEFAULT_HANDLER_A
  ));
  CHECK(!interrupt_vector_initialize(
    &state,
    mock_vector_scb(),
    NULL,
    &table,
    INITIAL_STACK_A,
    RESET_HANDLER_A,
    DEFAULT_HANDLER_A
  ));
  CHECK(!interrupt_vector_initialize(
    &state,
    mock_vector_scb(),
    mock_vector_nvic(),
    NULL,
    INITIAL_STACK_A,
    RESET_HANDLER_A,
    DEFAULT_HANDLER_A
  ));
  CHECK(!interrupt_vector_initialize(
    &state,
    mock_vector_scb(),
    mock_vector_nvic(),
    &table,
    UINT32_C(0x20002004),
    RESET_HANDLER_A,
    DEFAULT_HANDLER_A
  ));
  CHECK(!interrupt_vector_initialize(
    &state,
    mock_vector_scb(),
    mock_vector_nvic(),
    &table,
    INITIAL_STACK_A,
    RESET_HANDLER_A & ~INTERRUPT_VECTOR_HANDLER_THUMB_BIT,
    DEFAULT_HANDLER_A
  ));
  CHECK(!interrupt_vector_initialize(
    &state,
    mock_vector_scb(),
    mock_vector_nvic(),
    &table,
    INITIAL_STACK_A,
    RESET_HANDLER_A,
    0u
  ));
  mock_vector_set_table_address_override(UINT32_C(0x20000004));
  CHECK(!initialize(&state, &table));
  mock_vector_clear_table_address_override();

  CHECK(mock_vector_event_count() == 0u);
  CHECK(mock_vector_irq_save_count() == 0u);
  CHECK(mock_vector_irq_restore_count() == 0u);
  CHECK(state_equals(&state, &before));
  for (size_t index = 0u; index < INTERRUPT_VECTOR_ENTRY_COUNT; index++) {
    CHECK(table.entries[index] == table_before[index]);
  }
  CHECK(!mock_vector_invalid_access());
  return true;
}

static bool test_reinitialization_replaces_prior_configuration(void) {
  interrupt_vector_table_t table;
  interrupt_vector_state_t state = { 0 };

  mock_vector_reset();
  CHECK(initialize(&state, &table));
  mock_vector_set_enabled_irqs(INTERRUPT_VECTOR_EXTERNAL_MASK);
  mock_vector_set_pending_irqs(INTERRUPT_VECTOR_EXTERNAL_MASK);
  mock_vector_clear_log();

  CHECK(interrupt_vector_initialize(
    &state,
    mock_vector_scb(),
    mock_vector_nvic(),
    &table,
    INITIAL_STACK_B,
    RESET_HANDLER_B,
    DEFAULT_HANDLER_B
  ));
  CHECK(table_equals_value(
    &table,
    INITIAL_STACK_B,
    RESET_HANDLER_B,
    DEFAULT_HANDLER_B
  ));
  CHECK(mock_vector_enabled_irqs() == 0u);
  CHECK(mock_vector_pending_irqs() == 0u);
  CHECK(startup_events_match(
    interrupt_vector_table_address(&table),
    INITIAL_STACK_B,
    RESET_HANDLER_B,
    DEFAULT_HANDLER_B
  ));
  CHECK(mock_vector_irq_save_count() == 0u);
  CHECK(!mock_vector_invalid_access());
  return true;
}

static bool test_irq_installation_is_bounded_and_interrupt_safe(void) {
  interrupt_vector_table_t table;
  interrupt_vector_state_t state = { 0 };
  interrupt_vector_state_t uninitialized = { 0 };
  uint32_t previous_entry;

  mock_vector_reset();
  CHECK(initialize(&state, &table));
  mock_vector_clear_log();
  mock_vector_set_interrupts_enabled(false);

  CHECK(interrupt_vector_install_irq(&state, 7u, UINT32_C(0x08000901)));
  CHECK(table.entries[INTERRUPT_VECTOR_CORE_ENTRY_COUNT + 7u] ==
    UINT32_C(0x08000901));
  CHECK(mock_vector_event_count() == 4u);
  CHECK(event_matches(
    0u,
    MOCK_VECTOR_EVENT_IRQ_SAVE,
    SIZE_MAX,
    0u
  ));
  CHECK(event_matches(
    1u,
    MOCK_VECTOR_EVENT_TABLE_WRITE,
    INTERRUPT_VECTOR_CORE_ENTRY_COUNT + 7u,
    UINT32_C(0x08000901)
  ));
  CHECK(event_matches(2u, MOCK_VECTOR_EVENT_BARRIER, SIZE_MAX, 0u));
  CHECK(event_matches(
    3u,
    MOCK_VECTOR_EVENT_IRQ_RESTORE,
    SIZE_MAX,
    0u
  ));
  CHECK(!mock_vector_interrupts_enabled());
  CHECK(mock_vector_irq_save_count() == 1u);
  CHECK(mock_vector_irq_restore_count() == 1u);
  CHECK(mock_vector_last_irq_restore() == 0u);

  mock_vector_clear_log();
  mock_vector_set_interrupts_enabled(true);
  CHECK(interrupt_vector_install_irq(&state, 0u, UINT32_C(0x08000B01)));
  CHECK(table.entries[INTERRUPT_VECTOR_CORE_ENTRY_COUNT] ==
    UINT32_C(0x08000B01));
  CHECK(mock_vector_last_irq_restore() == UINT32_C(1));
  CHECK(mock_vector_interrupts_enabled());

  previous_entry = table.entries[INTERRUPT_VECTOR_CORE_ENTRY_COUNT];
  mock_vector_clear_log();
  CHECK(!interrupt_vector_install_irq(NULL, 0u, UINT32_C(0x08000D01)));
  CHECK(!interrupt_vector_install_irq(
    &uninitialized,
    0u,
    UINT32_C(0x08000D01)
  ));
  CHECK(!interrupt_vector_install_irq(
    &state,
    INTERRUPT_VECTOR_EXTERNAL_IRQ_COUNT,
    UINT32_C(0x08000D01)
  ));
  CHECK(!interrupt_vector_install_irq(&state, 0u, UINT32_C(0x08000D00)));
  CHECK(mock_vector_event_count() == 0u);
  CHECK(table.entries[INTERRUPT_VECTOR_CORE_ENTRY_COUNT] == previous_entry);
  CHECK(!mock_vector_invalid_access());
  return true;
}

static bool test_irq_enable_clears_stale_pending_before_unmasking(void) {
  interrupt_vector_table_t table;
  interrupt_vector_state_t state = { 0 };
  interrupt_vector_state_t uninitialized = { 0 };
  const uint32_t irq_bit = UINT32_C(1) << 3;

  mock_vector_reset();
  CHECK(initialize(&state, &table));
  mock_vector_set_pending_irqs(INTERRUPT_VECTOR_EXTERNAL_MASK);
  mock_vector_set_enabled_irqs(0u);
  mock_vector_clear_log();
  mock_vector_set_interrupts_enabled(false);

  CHECK(interrupt_vector_enable_irq(&state, 3u));
  CHECK(mock_vector_pending_irqs() ==
    (INTERRUPT_VECTOR_EXTERNAL_MASK & ~irq_bit));
  CHECK(mock_vector_enabled_irqs() == irq_bit);
  CHECK(mock_vector_event_count() == 4u);
  CHECK(event_matches(
    0u,
    MOCK_VECTOR_EVENT_IRQ_SAVE,
    SIZE_MAX,
    0u
  ));
  CHECK(event_matches(
    1u,
    MOCK_VECTOR_EVENT_NVIC_ICPR,
    SIZE_MAX,
    irq_bit
  ));
  CHECK(event_matches(
    2u,
    MOCK_VECTOR_EVENT_NVIC_ISER,
    SIZE_MAX,
    irq_bit
  ));
  CHECK(event_matches(
    3u,
    MOCK_VECTOR_EVENT_IRQ_RESTORE,
    SIZE_MAX,
    0u
  ));
  CHECK(!mock_vector_interrupts_enabled());
  CHECK(mock_vector_irq_save_count() == 1u);
  CHECK(mock_vector_irq_restore_count() == 1u);

  mock_vector_clear_log();
  CHECK(!interrupt_vector_enable_irq(NULL, 0u));
  CHECK(!interrupt_vector_enable_irq(&uninitialized, 0u));
  CHECK(!interrupt_vector_enable_irq(
    &state,
    INTERRUPT_VECTOR_EXTERNAL_IRQ_COUNT
  ));
  CHECK(mock_vector_event_count() == 0u);
  CHECK(mock_vector_pending_irqs() ==
    (INTERRUPT_VECTOR_EXTERNAL_MASK & ~irq_bit));
  CHECK(mock_vector_enabled_irqs() == irq_bit);
  CHECK(!mock_vector_invalid_access());
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "startup sequence and table layout", test_startup_sequence_and_layout },
    { "invalid initialization has no side effects", test_invalid_initialization_has_no_side_effects },
    { "reinitialization replaces prior configuration", test_reinitialization_replaces_prior_configuration },
    { "IRQ installation is bounded and interrupt safe", test_irq_installation_is_bounded_and_interrupt_safe },
    { "IRQ enable clears stale pending before unmasking", test_irq_enable_clears_stale_pending_before_unmasking },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) return 1;
    printf("ok - %s\n", tests[index].name);
  }
  return 0;
}

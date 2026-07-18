#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "deferred_work.h"
#include "mock_interrupt_work.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "failed: %s at line %d\n", #condition, __LINE__); \
      return false; \
    } \
  } while (0)

static bool expect_event(
  size_t index,
  mock_interrupt_work_event_t event,
  uint32_t value
) {
  CHECK(mock_interrupt_work_event_at(index) == event);
  CHECK(mock_interrupt_work_event_value(index) == value);
  return true;
}

static bool prepare_dispatcher(deferred_work_t *dispatcher) {
  mock_interrupt_work_reset();
  CHECK(deferred_work_init(dispatcher, mock_interrupt_work_latch()));
  CHECK(deferred_work_configure_sources(
    dispatcher,
    INTERRUPT_WORK_SOURCE_MASK
  ));
  mock_interrupt_work_clear_events();
  return true;
}

static bool test_init_rejects_invalid_and_orders_latch_setup(void) {
  mock_interrupt_work_reset();
  deferred_work_t dispatcher = { 0 };

  CHECK(!deferred_work_init(NULL, mock_interrupt_work_latch()));
  CHECK(!deferred_work_init(&dispatcher, NULL));
  CHECK(mock_interrupt_work_event_count() == 0u);

  CHECK(deferred_work_init(&dispatcher, mock_interrupt_work_latch()));
  CHECK(dispatcher.initialized);
  CHECK(dispatcher.latch == mock_interrupt_work_latch());
  CHECK(dispatcher.enabled_sources == 0u);
  CHECK(deferred_work_take(&dispatcher) == 0u);
  CHECK(mock_interrupt_work_event_count() == 2u);
  CHECK(expect_event(
    0u,
    MOCK_INTERRUPT_WORK_EVENT_ENABLE_WRITE,
    0u
  ));
  CHECK(expect_event(
    1u,
    MOCK_INTERRUPT_WORK_EVENT_STATUS_CLEAR,
    INTERRUPT_WORK_SOURCE_MASK
  ));
  CHECK(mock_interrupt_work_irq_save_count() == 0u);
  CHECK(mock_interrupt_work_irq_restore_count() == 0u);
  CHECK(!mock_interrupt_work_invalid_access());
  return true;
}

static bool test_configuration_is_critical_and_clears_stale_status(void) {
  mock_interrupt_work_reset();
  deferred_work_t dispatcher = { 0 };
  CHECK(deferred_work_init(&dispatcher, mock_interrupt_work_latch()));
  mock_interrupt_work_clear_events();

  const uint32_t saved_state = UINT32_C(0x13579BDE);
  mock_interrupt_work_set_irq_state(saved_state);
  mock_interrupt_work_raise(INTERRUPT_WORK_SOURCE_MASK);
  CHECK(deferred_work_configure_sources(
    &dispatcher,
    INTERRUPT_WORK_SOURCE_LOW
  ));
  CHECK(mock_interrupt_work_event_count() == 5u);
  CHECK(expect_event(
    0u,
    MOCK_INTERRUPT_WORK_EVENT_IRQ_SAVE,
    saved_state
  ));
  CHECK(expect_event(
    1u,
    MOCK_INTERRUPT_WORK_EVENT_ENABLE_WRITE,
    0u
  ));
  CHECK(expect_event(
    2u,
    MOCK_INTERRUPT_WORK_EVENT_STATUS_CLEAR,
    INTERRUPT_WORK_SOURCE_MASK
  ));
  CHECK(expect_event(
    3u,
    MOCK_INTERRUPT_WORK_EVENT_ENABLE_WRITE,
    INTERRUPT_WORK_SOURCE_LOW
  ));
  CHECK(expect_event(
    4u,
    MOCK_INTERRUPT_WORK_EVENT_IRQ_RESTORE,
    saved_state
  ));
  CHECK(mock_interrupt_work_status() == 0u);
  CHECK(mock_interrupt_work_enable() == INTERRUPT_WORK_SOURCE_LOW);
  CHECK(mock_interrupt_work_irq_state() == saved_state);
  CHECK(mock_interrupt_work_last_irq_restore() == saved_state);

  mock_interrupt_work_clear_events();
  CHECK(!deferred_work_configure_sources(&dispatcher, UINT32_C(4)));
  CHECK(mock_interrupt_work_event_count() == 0u);
  CHECK(mock_interrupt_work_enable() == INTERRUPT_WORK_SOURCE_LOW);
  CHECK(!mock_interrupt_work_invalid_access());
  return true;
}

static bool test_irqs_capture_only_their_source_and_coalesce(void) {
  deferred_work_t dispatcher = { 0 };
  CHECK(prepare_dispatcher(&dispatcher));

  const size_t saves_before = mock_interrupt_work_irq_save_count();
  const size_t restores_before = mock_interrupt_work_irq_restore_count();
  mock_interrupt_work_raise(INTERRUPT_WORK_SOURCE_MASK);
  deferred_work_low_irq(&dispatcher);
  CHECK(mock_interrupt_work_event_count() == 2u);
  CHECK(expect_event(
    0u,
    MOCK_INTERRUPT_WORK_EVENT_STATUS_READ,
    INTERRUPT_WORK_SOURCE_MASK
  ));
  CHECK(expect_event(
    1u,
    MOCK_INTERRUPT_WORK_EVENT_STATUS_CLEAR,
    INTERRUPT_WORK_SOURCE_LOW
  ));
  CHECK(mock_interrupt_work_status() == INTERRUPT_WORK_SOURCE_HIGH);
  CHECK(deferred_work_take(&dispatcher) == INTERRUPT_WORK_SOURCE_LOW);

  deferred_work_high_irq(&dispatcher);
  CHECK(mock_interrupt_work_status() == 0u);
  CHECK(deferred_work_take(&dispatcher) == INTERRUPT_WORK_SOURCE_HIGH);
  CHECK(deferred_work_take(&dispatcher) == 0u);

  mock_interrupt_work_clear_events();
  mock_interrupt_work_raise(INTERRUPT_WORK_SOURCE_LOW);
  deferred_work_low_irq(&dispatcher);
  mock_interrupt_work_raise(INTERRUPT_WORK_SOURCE_LOW);
  deferred_work_low_irq(&dispatcher);
  CHECK(deferred_work_take(&dispatcher) == INTERRUPT_WORK_SOURCE_LOW);
  CHECK(deferred_work_take(&dispatcher) == 0u);
  CHECK(mock_interrupt_work_irq_save_count() == saves_before);
  CHECK(mock_interrupt_work_irq_restore_count() == restores_before);
  CHECK(!mock_interrupt_work_invalid_access());
  return true;
}

static void inject_high_priority_irq(void *context) {
  deferred_work_t *const dispatcher = context;
  mock_interrupt_work_raise(INTERRUPT_WORK_SOURCE_HIGH);
  deferred_work_high_irq(dispatcher);
}

static bool test_nested_high_priority_irq_does_not_lose_low_work(void) {
  deferred_work_t dispatcher = { 0 };
  CHECK(prepare_dispatcher(&dispatcher));

  mock_interrupt_work_raise(INTERRUPT_WORK_SOURCE_LOW);
  mock_interrupt_work_set_next_status_read_hook(
    inject_high_priority_irq,
    &dispatcher
  );
  deferred_work_low_irq(&dispatcher);

  CHECK(mock_interrupt_work_status() == 0u);
  CHECK(deferred_work_take(&dispatcher) == INTERRUPT_WORK_SOURCE_MASK);
  CHECK(deferred_work_take(&dispatcher) == 0u);
  CHECK(mock_interrupt_work_irq_save_count() == 1u);
  CHECK(mock_interrupt_work_irq_restore_count() == 1u);
  CHECK(!mock_interrupt_work_invalid_access());
  return true;
}

static bool test_reconfigure_preserves_deferred_work_and_invalid_calls(void) {
  deferred_work_t dispatcher = { 0 };
  CHECK(prepare_dispatcher(&dispatcher));

  mock_interrupt_work_raise(INTERRUPT_WORK_SOURCE_LOW);
  deferred_work_low_irq(&dispatcher);
  mock_interrupt_work_set_irq_state(UINT32_C(0x2468ACE0));
  CHECK(deferred_work_configure_sources(
    &dispatcher,
    INTERRUPT_WORK_SOURCE_HIGH
  ));
  CHECK(mock_interrupt_work_enable() == INTERRUPT_WORK_SOURCE_HIGH);
  CHECK(deferred_work_take(&dispatcher) == INTERRUPT_WORK_SOURCE_LOW);

  CHECK(deferred_work_configure_sources(&dispatcher, 0u));
  CHECK(mock_interrupt_work_enable() == 0u);
  CHECK(deferred_work_take(NULL) == 0u);
  mock_interrupt_work_clear_events();
  deferred_work_low_irq(NULL);
  CHECK(mock_interrupt_work_event_count() == 0u);

  deferred_work_t uninitialized = { 0 };
  deferred_work_high_irq(&uninitialized);
  CHECK(deferred_work_take(&uninitialized) == 0u);
  CHECK(mock_interrupt_work_event_count() == 0u);
  CHECK(!mock_interrupt_work_invalid_access());
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "initialization", test_init_rejects_invalid_and_orders_latch_setup },
    { "critical configuration", test_configuration_is_critical_and_clears_stale_status },
    { "source capture and coalescing", test_irqs_capture_only_their_source_and_coalesce },
    { "nested priority interleaving", test_nested_high_priority_irq_does_not_lose_low_work },
    { "reconfiguration and invalid calls", test_reconfigure_preserves_deferred_work_and_invalid_calls },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) {
      fprintf(stderr, "test failed: %s\n", tests[index].name);
      return 1;
    }
  }

  puts("all interrupt deferred-work tests passed");
  return 0;
}

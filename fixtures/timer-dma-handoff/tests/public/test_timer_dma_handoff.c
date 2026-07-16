#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "mock_timer_dma_handoff.h"
#include "timer_dma_handoff.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
              __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

typedef struct {
  mock_timer_dma_event_t event;
  uint32_t value;
} expected_event_t;

static bool initialize(
  timer_dma_t *driver,
  uint16_t period_ticks,
  uint16_t initial_compare_ticks
) {
  return timer_dma_init(
    driver,
    mock_timer0(),
    mock_dma0(),
    period_ticks,
    initial_compare_ticks
  );
}

static bool state_equals(const timer_dma_t *left, const timer_dma_t *right) {
  return left->timer == right->timer && left->dma == right->dma &&
    left->samples == right->samples && left->sample_count == right->sample_count &&
    left->period_ticks == right->period_ticks &&
    left->last_compare_ticks == right->last_compare_ticks &&
    left->error_flags == right->error_flags && left->owner == right->owner &&
    left->result == right->result && left->initialized == right->initialized;
}

static bool events_match_from(
  size_t offset,
  const expected_event_t *expected,
  size_t expected_count
) {
  if (mock_timer_dma_event_count() != offset + expected_count) return false;
  for (size_t index = 0u; index < expected_count; index++) {
    if (
      mock_timer_dma_event_at(offset + index) != expected[index].event ||
      mock_timer_dma_event_value(offset + index) != expected[index].value
    ) {
      return false;
    }
  }
  return true;
}

static bool test_initialization_validation_and_cpu_baseline(void) {
  timer_dma_t driver = {
    .timer = (volatile timer0_registers_t *)(uintptr_t)UINT32_C(1),
    .dma = (volatile dma0_registers_t *)(uintptr_t)UINT32_C(1),
    .samples = (const uint16_t *)(uintptr_t)UINT32_C(1),
    .sample_count = 3u,
    .period_ticks = UINT16_C(20),
    .last_compare_ticks = UINT16_C(8),
    .error_flags = DMA0_STATUS_ERROR,
    .owner = TIMER_DMA_OWNER_RECOVERY_REQUIRED,
    .result = TIMER_DMA_RESULT_ERROR,
    .initialized = true,
  };
  const timer_dma_t before = driver;
  const expected_event_t expected[] = {
    { MOCK_TIMER_DMA_EVENT_TIMER_CONTROL_WRITE, 0u },
    { MOCK_TIMER_DMA_EVENT_DMA_CONTROL_WRITE, 0u },
    { MOCK_TIMER_DMA_EVENT_DMA_STATUS_CLEAR_WRITE, DMA0_STATUS_ALL },
    { MOCK_TIMER_DMA_EVENT_TIMER_PERIOD_WRITE, UINT16_C(20) },
    { MOCK_TIMER_DMA_EVENT_TIMER_COMPARE_SHADOW_WRITE, UINT16_C(4) },
    { MOCK_TIMER_DMA_EVENT_TIMER_CONTROL_WRITE, TIMER0_CONTROL_READY },
  };

  mock_timer_dma_reset();
  CHECK(!timer_dma_init(NULL, mock_timer0(), mock_dma0(), UINT16_C(20), 4u));
  CHECK(!timer_dma_init(&driver, NULL, mock_dma0(), UINT16_C(20), 4u));
  CHECK(!timer_dma_init(&driver, mock_timer0(), NULL, UINT16_C(20), 4u));
  CHECK(!timer_dma_init(&driver, mock_timer0(), mock_dma0(), UINT16_C(1), 0u));
  CHECK(!timer_dma_init(
    &driver,
    mock_timer0(),
    mock_dma0(),
    (uint16_t)(TIMER0_MAX_PERIOD_TICKS + UINT16_C(1)),
    0u
  ));
  CHECK(!timer_dma_init(
    &driver,
    mock_timer0(),
    mock_dma0(),
    UINT16_C(20),
    UINT16_C(21)
  ));
  CHECK(state_equals(&driver, &before));
  CHECK(mock_timer_dma_event_count() == 0u);

  CHECK(initialize(&driver, UINT16_C(20), UINT16_C(4)));
  CHECK(events_match_from(0u, expected, sizeof(expected) / sizeof(expected[0])));
  CHECK(driver.timer == mock_timer0());
  CHECK(driver.dma == mock_dma0());
  CHECK(driver.samples == NULL);
  CHECK(driver.sample_count == 0u);
  CHECK(driver.period_ticks == UINT16_C(20));
  CHECK(driver.last_compare_ticks == UINT16_C(4));
  CHECK(driver.error_flags == 0u);
  CHECK(driver.owner == TIMER_DMA_OWNER_CPU);
  CHECK(driver.result == TIMER_DMA_RESULT_NONE);
  CHECK(driver.initialized);
  CHECK(mock_timer0_control() == TIMER0_CONTROL_READY);
  CHECK(mock_timer0_period() == UINT16_C(20));
  CHECK(mock_timer0_compare_shadow() == UINT16_C(4));
  CHECK(mock_timer0_compare_active() == UINT16_C(4));
  CHECK(mock_dma0_control() == 0u);
  CHECK(mock_dma0_status() == 0u);
  CHECK(!mock_timer_dma_invalid_access());
  return true;
}

static bool test_start_hands_timer_to_dma_and_preserves_interrupt_state(void) {
  const uint16_t samples[] = { UINT16_C(1), UINT16_C(8), UINT16_C(20) };
  const uint16_t invalid_samples[] = { UINT16_C(1), UINT16_C(21) };
  timer_dma_t driver = { 0 };
  const expected_event_t start_events[] = {
    { MOCK_TIMER_DMA_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xA5) },
    { MOCK_TIMER_DMA_EVENT_DMA_STATUS_CLEAR_WRITE, DMA0_STATUS_ALL },
    { MOCK_TIMER_DMA_EVENT_DMA_SOURCE_WRITE, 0u },
    { MOCK_TIMER_DMA_EVENT_DMA_DESTINATION_WRITE, 0u },
    { MOCK_TIMER_DMA_EVENT_DMA_COUNT_WRITE, UINT32_C(3) },
    { MOCK_TIMER_DMA_EVENT_DMA_CONTROL_WRITE, DMA0_CHANNEL_CONTROL_READY },
    { MOCK_TIMER_DMA_EVENT_TIMER_CONTROL_WRITE, TIMER0_CONTROL_DMA_OWNED },
    { MOCK_TIMER_DMA_EVENT_IRQ_RESTORE, UINT32_C(0xA5) },
  };
  const expected_event_t rejected_start_events[] = {
    { MOCK_TIMER_DMA_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xA5) },
    { MOCK_TIMER_DMA_EVENT_IRQ_RESTORE, UINT32_C(0xA5) },
  };
  size_t offset;

  mock_timer_dma_reset();
  CHECK(initialize(&driver, UINT16_C(20), UINT16_C(4)));
  offset = mock_timer_dma_event_count();
  CHECK(!timer_dma_start(NULL, samples, 1u));
  CHECK(!timer_dma_start(&driver, NULL, 1u));
  CHECK(!timer_dma_start(&driver, samples, 0u));
  CHECK(!timer_dma_start(&driver, samples, TIMER_DMA_MAX_SAMPLES + 1u));
  CHECK(!timer_dma_start(&driver, invalid_samples, 2u));
  CHECK(mock_timer_dma_event_count() == offset);

  mock_timer_dma_set_irq_state(UINT32_C(0xA5));
  offset = mock_timer_dma_event_count();
  CHECK(timer_dma_start(&driver, samples, 3u));
  CHECK(events_match_from(
    offset,
    start_events,
    sizeof(start_events) / sizeof(start_events[0])
  ));
  CHECK(driver.samples == samples);
  CHECK(driver.sample_count == 3u);
  CHECK(driver.owner == TIMER_DMA_OWNER_DMA);
  CHECK(driver.result == TIMER_DMA_RESULT_NONE);
  CHECK(mock_timer0_control() == TIMER0_CONTROL_DMA_OWNED);
  CHECK(mock_dma0_source() == timer_dma_buffer_address(samples));
  CHECK(mock_dma0_destination() == timer0_compare_dma_address(mock_timer0()));
  CHECK(mock_dma0_count() == UINT32_C(3));
  CHECK(mock_dma0_transferred() == 0u);
  CHECK(mock_dma0_control() == DMA0_CHANNEL_CONTROL_READY);
  CHECK(mock_timer_dma_irq_state() == UINT32_C(0xA5));

  offset = mock_timer_dma_event_count();
  CHECK(!timer_dma_start(&driver, samples, 1u));
  CHECK(events_match_from(
    offset,
    rejected_start_events,
    sizeof(rejected_start_events) / sizeof(rejected_start_events[0])
  ));
  CHECK(!mock_timer_dma_invalid_access());
  return true;
}

static bool test_completion_returns_cpu_ownership_after_result_acknowledgement(void) {
  const uint16_t samples[] = { UINT16_C(2), UINT16_C(7), UINT16_C(13) };
  const uint16_t maximum_samples[TIMER_DMA_MAX_SAMPLES] = {
    UINT16_C(0), UINT16_C(1), UINT16_C(2), UINT16_C(3),
    UINT16_C(4), UINT16_C(5), UINT16_C(6), UINT16_C(7),
  };
  timer_dma_t driver = { 0 };
  uint32_t error_flags = UINT32_MAX;
  const expected_event_t complete_events[] = {
    { MOCK_TIMER_DMA_EVENT_DMA_STATUS_READ, DMA0_STATUS_COMPLETE },
    { MOCK_TIMER_DMA_EVENT_DMA_STATUS_CLEAR_WRITE, DMA0_STATUS_COMPLETE },
    { MOCK_TIMER_DMA_EVENT_TIMER_CONTROL_WRITE, TIMER0_CONTROL_READY },
    { MOCK_TIMER_DMA_EVENT_DMA_CONTROL_WRITE, 0u },
    { MOCK_TIMER_DMA_EVENT_TIMER_COMPARE_ACTIVE_READ, UINT16_C(13) },
  };
  const expected_event_t rejected_start_events[] = {
    { MOCK_TIMER_DMA_EVENT_IRQ_SAVE_DISABLE, UINT32_C(1) },
    { MOCK_TIMER_DMA_EVENT_IRQ_RESTORE, UINT32_C(1) },
  };
  const expected_event_t take_result_events[] = {
    { MOCK_TIMER_DMA_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0x3C) },
    { MOCK_TIMER_DMA_EVENT_IRQ_RESTORE, UINT32_C(0x3C) },
  };
  size_t offset;

  mock_timer_dma_reset();
  CHECK(initialize(&driver, UINT16_C(20), UINT16_C(4)));
  CHECK(timer_dma_start(&driver, samples, 3u));
  CHECK(mock_timer_dma_tick());
  CHECK(mock_timer0_compare_active() == UINT16_C(2));
  CHECK(mock_timer_dma_tick());
  CHECK(mock_timer0_compare_active() == UINT16_C(7));
  CHECK(mock_timer_dma_tick());
  CHECK(mock_timer0_compare_active() == UINT16_C(13));
  CHECK(mock_dma0_status() == DMA0_STATUS_COMPLETE);
  CHECK(mock_dma0_control() == 0u);

  offset = mock_timer_dma_event_count();
  timer_dma_irq(&driver);
  CHECK(events_match_from(
    offset,
    complete_events,
    sizeof(complete_events) / sizeof(complete_events[0])
  ));
  CHECK(driver.owner == TIMER_DMA_OWNER_CPU);
  CHECK(driver.result == TIMER_DMA_RESULT_COMPLETE);
  CHECK(driver.samples == NULL);
  CHECK(driver.sample_count == 0u);
  CHECK(driver.last_compare_ticks == UINT16_C(13));
  CHECK(mock_timer0_control() == TIMER0_CONTROL_READY);
  CHECK(mock_dma0_status() == 0u);

  offset = mock_timer_dma_event_count();
  CHECK(!timer_dma_start(&driver, samples, 1u));
  CHECK(events_match_from(
    offset,
    rejected_start_events,
    sizeof(rejected_start_events) / sizeof(rejected_start_events[0])
  ));
  mock_timer_dma_set_irq_state(UINT32_C(0x3C));
  offset = mock_timer_dma_event_count();
  CHECK(timer_dma_take_result(&driver, &error_flags) ==
    TIMER_DMA_RESULT_COMPLETE);
  CHECK(events_match_from(
    offset,
    take_result_events,
    sizeof(take_result_events) / sizeof(take_result_events[0])
  ));
  CHECK(error_flags == 0u);
  CHECK(driver.result == TIMER_DMA_RESULT_NONE);
  CHECK(timer_dma_start(
    &driver,
    maximum_samples,
    TIMER_DMA_MAX_SAMPLES
  ));
  CHECK(driver.sample_count == TIMER_DMA_MAX_SAMPLES);
  CHECK(mock_dma0_count() == TIMER_DMA_MAX_SAMPLES);
  CHECK(!mock_timer_dma_invalid_access());
  return true;
}

static bool test_abort_requires_acknowledgement_and_deterministic_recovery(void) {
  const uint16_t samples[] = { UINT16_C(5), UINT16_C(9), UINT16_C(15) };
  timer_dma_t driver = { 0 };
  uint32_t error_flags = UINT32_MAX;
  const expected_event_t abort_events[] = {
    { MOCK_TIMER_DMA_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0x5A) },
    { MOCK_TIMER_DMA_EVENT_TIMER_CONTROL_WRITE, TIMER0_CONTROL_READY },
    { MOCK_TIMER_DMA_EVENT_DMA_CONTROL_WRITE, DMA0_CHANNEL_CONTROL_ABORT },
    { MOCK_TIMER_DMA_EVENT_IRQ_RESTORE, UINT32_C(0x5A) },
  };
  const expected_event_t abort_irq_events[] = {
    { MOCK_TIMER_DMA_EVENT_DMA_STATUS_READ, DMA0_STATUS_ABORTED },
    { MOCK_TIMER_DMA_EVENT_DMA_STATUS_CLEAR_WRITE, DMA0_STATUS_ABORTED },
    { MOCK_TIMER_DMA_EVENT_TIMER_CONTROL_WRITE, TIMER0_CONTROL_READY },
    { MOCK_TIMER_DMA_EVENT_DMA_CONTROL_WRITE, 0u },
    { MOCK_TIMER_DMA_EVENT_TIMER_COMPARE_ACTIVE_READ, UINT16_C(5) },
  };
  const expected_event_t rejected_recovery_events[] = {
    { MOCK_TIMER_DMA_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0x5A) },
    { MOCK_TIMER_DMA_EVENT_IRQ_RESTORE, UINT32_C(0x5A) },
  };
  const expected_event_t recovery_events[] = {
    { MOCK_TIMER_DMA_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0x5A) },
    { MOCK_TIMER_DMA_EVENT_TIMER_CONTROL_WRITE, 0u },
    { MOCK_TIMER_DMA_EVENT_DMA_CONTROL_WRITE, 0u },
    { MOCK_TIMER_DMA_EVENT_DMA_STATUS_CLEAR_WRITE, DMA0_STATUS_ALL },
    { MOCK_TIMER_DMA_EVENT_TIMER_PERIOD_WRITE, UINT16_C(20) },
    { MOCK_TIMER_DMA_EVENT_TIMER_COMPARE_SHADOW_WRITE, UINT16_C(5) },
    { MOCK_TIMER_DMA_EVENT_TIMER_CONTROL_WRITE, TIMER0_CONTROL_READY },
    { MOCK_TIMER_DMA_EVENT_IRQ_RESTORE, UINT32_C(0x5A) },
  };
  size_t offset;

  mock_timer_dma_reset();
  CHECK(initialize(&driver, UINT16_C(20), UINT16_C(4)));
  CHECK(timer_dma_start(&driver, samples, 3u));
  CHECK(mock_timer_dma_tick());
  CHECK(mock_timer0_compare_active() == UINT16_C(5));
  mock_timer_dma_set_irq_state(UINT32_C(0x5A));

  offset = mock_timer_dma_event_count();
  CHECK(timer_dma_abort(&driver));
  CHECK(events_match_from(
    offset,
    abort_events,
    sizeof(abort_events) / sizeof(abort_events[0])
  ));
  CHECK(driver.owner == TIMER_DMA_OWNER_ABORTING);
  CHECK(driver.result == TIMER_DMA_RESULT_NONE);
  CHECK(mock_dma0_status() == DMA0_STATUS_ABORTED);
  CHECK(!timer_dma_abort(&driver));

  offset = mock_timer_dma_event_count();
  timer_dma_irq(&driver);
  CHECK(events_match_from(
    offset,
    abort_irq_events,
    sizeof(abort_irq_events) / sizeof(abort_irq_events[0])
  ));
  CHECK(driver.owner == TIMER_DMA_OWNER_RECOVERY_REQUIRED);
  CHECK(driver.result == TIMER_DMA_RESULT_ABORTED);
  CHECK(driver.last_compare_ticks == UINT16_C(5));
  CHECK(mock_timer0_control() == TIMER0_CONTROL_READY);
  CHECK(mock_timer0_compare_active() == UINT16_C(5));

  offset = mock_timer_dma_event_count();
  CHECK(!timer_dma_recover(&driver));
  CHECK(events_match_from(
    offset,
    rejected_recovery_events,
    sizeof(rejected_recovery_events) / sizeof(rejected_recovery_events[0])
  ));
  CHECK(timer_dma_take_result(&driver, &error_flags) ==
    TIMER_DMA_RESULT_ABORTED);
  CHECK(error_flags == 0u);
  CHECK(driver.result == TIMER_DMA_RESULT_NONE);

  offset = mock_timer_dma_event_count();
  CHECK(timer_dma_recover(&driver));
  CHECK(events_match_from(
    offset,
    recovery_events,
    sizeof(recovery_events) / sizeof(recovery_events[0])
  ));
  CHECK(driver.owner == TIMER_DMA_OWNER_CPU);
  CHECK(mock_timer0_period() == UINT16_C(20));
  CHECK(mock_timer0_compare_shadow() == UINT16_C(5));
  CHECK(mock_timer0_compare_active() == UINT16_C(5));
  CHECK(timer_dma_start(&driver, samples, 1u));
  CHECK(!mock_timer_dma_invalid_access());
  return true;
}

static bool test_error_has_terminal_priority_and_requires_recovery(void) {
  const uint16_t samples[] = { UINT16_C(6), UINT16_C(12) };
  timer_dma_t driver = { 0 };
  uint32_t error_flags = 0u;
  const expected_event_t error_events[] = {
    {
      MOCK_TIMER_DMA_EVENT_DMA_STATUS_READ,
      DMA0_STATUS_COMPLETE | DMA0_STATUS_ERROR | DMA0_STATUS_ABORTED,
    },
    {
      MOCK_TIMER_DMA_EVENT_DMA_STATUS_CLEAR_WRITE,
      DMA0_STATUS_COMPLETE | DMA0_STATUS_ERROR | DMA0_STATUS_ABORTED,
    },
    { MOCK_TIMER_DMA_EVENT_TIMER_CONTROL_WRITE, TIMER0_CONTROL_READY },
    { MOCK_TIMER_DMA_EVENT_DMA_CONTROL_WRITE, 0u },
    { MOCK_TIMER_DMA_EVENT_TIMER_COMPARE_ACTIVE_READ, UINT16_C(6) },
  };
  size_t offset;

  mock_timer_dma_reset();
  CHECK(initialize(&driver, UINT16_C(20), UINT16_C(4)));
  CHECK(timer_dma_start(&driver, samples, 2u));
  CHECK(mock_timer_dma_tick());
  mock_timer_dma_set_status(
    DMA0_STATUS_COMPLETE | DMA0_STATUS_ERROR | DMA0_STATUS_ABORTED
  );

  offset = mock_timer_dma_event_count();
  timer_dma_irq(&driver);
  CHECK(events_match_from(
    offset,
    error_events,
    sizeof(error_events) / sizeof(error_events[0])
  ));
  CHECK(driver.owner == TIMER_DMA_OWNER_RECOVERY_REQUIRED);
  CHECK(driver.result == TIMER_DMA_RESULT_ERROR);
  CHECK(driver.error_flags == DMA0_STATUS_ERROR);
  CHECK(driver.last_compare_ticks == UINT16_C(6));
  CHECK(!timer_dma_start(&driver, samples, 1u));
  CHECK(timer_dma_take_result(&driver, &error_flags) == TIMER_DMA_RESULT_ERROR);
  CHECK(error_flags == DMA0_STATUS_ERROR);
  CHECK(timer_dma_recover(&driver));
  CHECK(driver.owner == TIMER_DMA_OWNER_CPU);
  CHECK(timer_dma_start(&driver, samples, 1u));
  CHECK(!mock_timer_dma_invalid_access());
  return true;
}

static bool test_invalid_calls_and_stale_cpu_irq_have_no_side_effects(void) {
  const uint16_t samples[] = { UINT16_C(3) };
  timer_dma_t driver = { 0 };
  const timer_dma_t before = driver;
  uint32_t error_flags = UINT32_MAX;
  size_t offset;

  mock_timer_dma_reset();
  CHECK(!timer_dma_start(NULL, samples, 1u));
  CHECK(!timer_dma_start(&driver, samples, 1u));
  CHECK(!timer_dma_abort(NULL));
  CHECK(!timer_dma_abort(&driver));
  CHECK(!timer_dma_recover(NULL));
  CHECK(!timer_dma_recover(&driver));
  CHECK(timer_dma_take_result(NULL, &error_flags) == TIMER_DMA_RESULT_NONE);
  CHECK(timer_dma_take_result(&driver, &error_flags) == TIMER_DMA_RESULT_NONE);
  timer_dma_irq(NULL);
  timer_dma_irq(&driver);
  CHECK(state_equals(&driver, &before));
  CHECK(mock_timer_dma_event_count() == 0u);

  CHECK(initialize(&driver, UINT16_C(20), UINT16_C(4)));
  mock_timer_dma_set_status(DMA0_STATUS_COMPLETE);
  offset = mock_timer_dma_event_count();
  timer_dma_irq(&driver);
  CHECK(mock_timer_dma_event_count() == offset);
  CHECK(mock_dma0_status() == DMA0_STATUS_COMPLETE);
  CHECK(timer_dma_take_result(&driver, NULL) == TIMER_DMA_RESULT_NONE);
  CHECK(mock_timer_dma_event_count() == offset);
  CHECK(timer_dma_start(&driver, samples, 1u));
  CHECK(mock_dma0_status() == 0u);
  CHECK(!mock_timer_dma_invalid_access());
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "initialization and CPU baseline", test_initialization_validation_and_cpu_baseline },
    { "DMA ownership handoff", test_start_hands_timer_to_dma_and_preserves_interrupt_state },
    { "completion handoff", test_completion_returns_cpu_ownership_after_result_acknowledgement },
    { "abort recovery", test_abort_requires_acknowledgement_and_deterministic_recovery },
    { "error priority", test_error_has_terminal_priority_and_requires_recovery },
    { "invalid calls", test_invalid_calls_and_stale_cpu_irq_have_no_side_effects },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) {
      fprintf(stderr, "failed: %s\n", tests[index].name);
      return 1;
    }
  }

  printf("Timer-DMA handoff public tests passed (%zu tests).\n",
    sizeof(tests) / sizeof(tests[0]));
  return 0;
}

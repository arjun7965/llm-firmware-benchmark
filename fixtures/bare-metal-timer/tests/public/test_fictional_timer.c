#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "fictional_timer.h"
#include "mock_mmio.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
              __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

typedef struct {
  uint32_t control;
  uint32_t prescaler;
  uint32_t reload;
  uint32_t count;
  uint32_t status;
  uint32_t irq_clear;
} register_snapshot_t;

static register_snapshot_t snapshot(
  const volatile timer0_registers_t *timer
) {
  return (register_snapshot_t) {
    .control = timer->control,
    .prescaler = timer->prescaler,
    .reload = timer->reload,
    .count = timer->count,
    .status = timer->status,
    .irq_clear = timer->irq_clear,
  };
}

static bool snapshots_equal(
  register_snapshot_t left,
  register_snapshot_t right
) {
  return left.control == right.control &&
    left.prescaler == right.prescaler &&
    left.reload == right.reload &&
    left.count == right.count &&
    left.status == right.status &&
    left.irq_clear == right.irq_clear;
}

static bool check_write(size_t index, uint32_t offset, uint32_t value) {
  return mock_timer0_write_offset(index) == offset &&
    mock_timer0_write_value(index) == value;
}

static bool test_standard_configuration(void) {
  timer0_registers_t timer;
  mock_timer0_reset(&timer, UINT32_C(0xFFFFFFFF));

  CHECK(timer0_configure_periodic(
    &timer,
    UINT32_C(48000000),
    UINT32_C(1000)
  ));
  CHECK(timer.control == (
    TIMER0_CONTROL_ENABLE |
    TIMER0_CONTROL_PERIODIC |
    TIMER0_CONTROL_IRQ_ENABLE
  ));
  CHECK(timer.prescaler == UINT32_C(47));
  CHECK(timer.reload == UINT32_C(999));
  CHECK(timer.count == UINT32_C(0xFFFFFFFF));
  CHECK(
    (timer.status & TIMER0_STATUS_IRQ_PENDING) == 0u
  );
  CHECK(timer.irq_clear == TIMER0_STATUS_IRQ_PENDING);
  CHECK(!mock_timer0_invalid_access());
  return true;
}

static bool test_configuration_write_order(void) {
  timer0_registers_t timer;
  mock_timer0_reset(&timer, TIMER0_STATUS_IRQ_PENDING);

  CHECK(timer0_configure_periodic(
    &timer,
    UINT32_C(8000000),
    UINT32_C(250)
  ));
  CHECK(mock_timer0_write_count() == 5u);
  CHECK(check_write(0u, TIMER0_CONTROL_OFFSET, 0u));
  CHECK(check_write(1u, TIMER0_PRESCALER_OFFSET, UINT32_C(7)));
  CHECK(check_write(2u, TIMER0_RELOAD_OFFSET, UINT32_C(249)));
  CHECK(check_write(
    3u,
    TIMER0_IRQ_CLEAR_OFFSET,
    TIMER0_STATUS_IRQ_PENDING
  ));
  CHECK(check_write(
    4u,
    TIMER0_CONTROL_OFFSET,
    TIMER0_CONTROL_ENABLE |
      TIMER0_CONTROL_PERIODIC |
      TIMER0_CONTROL_IRQ_ENABLE
  ));
  return true;
}

static bool test_boundary_configurations(void) {
  timer0_registers_t timer;
  mock_timer0_reset(&timer, 0u);
  CHECK(timer0_configure_periodic(
    &timer,
    TIMER0_TICK_HZ,
    UINT32_C(1)
  ));
  CHECK(timer.prescaler == 0u);
  CHECK(timer.reload == 0u);

  mock_timer0_reset(&timer, 0u);
  CHECK(timer0_configure_periodic(
    &timer,
    UINT32_C(256000000),
    TIMER0_RELOAD_MAX + 1u
  ));
  CHECK(timer.prescaler == TIMER0_PRESCALER_MAX);
  CHECK(timer.reload == TIMER0_RELOAD_MAX);
  return true;
}

static bool reject_without_writes(
  uint32_t clock_hz,
  uint32_t period_us
) {
  timer0_registers_t timer;
  mock_timer0_reset(&timer, UINT32_C(0xA5A5A5A5));
  const register_snapshot_t before = snapshot(&timer);
  if (timer0_configure_periodic(&timer, clock_hz, period_us)) return false;
  return mock_timer0_write_count() == 0u &&
    snapshots_equal(before, snapshot(&timer)) &&
    !mock_timer0_invalid_access();
}

static bool test_invalid_inputs_leave_registers_unchanged(void) {
  mock_timer0_clear_log();
  CHECK(!timer0_configure_periodic(
    NULL,
    UINT32_C(1000000),
    UINT32_C(1)
  ));
  CHECK(mock_timer0_write_count() == 0u);
  CHECK(!mock_timer0_invalid_access());
  CHECK(reject_without_writes(0u, UINT32_C(1)));
  CHECK(reject_without_writes(UINT32_C(1500000), UINT32_C(1)));
  CHECK(reject_without_writes(UINT32_C(257000000), UINT32_C(1)));
  CHECK(reject_without_writes(UINT32_C(1000000), 0u));
  CHECK(reject_without_writes(
    UINT32_C(1000000),
    TIMER0_RELOAD_MAX + 2u
  ));
  return true;
}

static bool test_reconfiguration_replaces_programmed_values(void) {
  timer0_registers_t timer;
  mock_timer0_reset(&timer, 0u);
  CHECK(timer0_configure_periodic(
    &timer,
    UINT32_C(8000000),
    UINT32_C(250)
  ));
  mock_timer0_set_status(&timer, TIMER0_STATUS_IRQ_PENDING);
  mock_timer0_clear_log();

  CHECK(timer0_configure_periodic(
    &timer,
    UINT32_C(72000000),
    UINT32_C(2000)
  ));
  CHECK(timer.prescaler == UINT32_C(71));
  CHECK(timer.reload == UINT32_C(1999));
  CHECK((timer.status & TIMER0_STATUS_IRQ_PENDING) == 0u);
  CHECK(mock_timer0_write_count() == 5u);
  return true;
}

static bool test_stop_writes_only_control(void) {
  timer0_registers_t timer;
  mock_timer0_reset(&timer, UINT32_C(0x12345678));
  const register_snapshot_t before = snapshot(&timer);

  timer0_stop(&timer);
  CHECK(timer.control == 0u);
  CHECK(timer.prescaler == before.prescaler);
  CHECK(timer.reload == before.reload);
  CHECK(timer.count == before.count);
  CHECK(timer.status == before.status);
  CHECK(timer.irq_clear == before.irq_clear);
  CHECK(mock_timer0_write_count() == 1u);
  CHECK(check_write(0u, TIMER0_CONTROL_OFFSET, 0u));

  mock_timer0_clear_log();
  timer0_stop(NULL);
  CHECK(mock_timer0_write_count() == 0u);
  CHECK(!mock_timer0_invalid_access());
  return true;
}

static bool test_interrupt_query_and_acknowledge(void) {
  timer0_registers_t timer;
  mock_timer0_reset(&timer, UINT32_C(0xA4));
  CHECK(!timer0_irq_pending(&timer));
  CHECK(timer.status == UINT32_C(0xA4));
  CHECK(mock_timer0_write_count() == 0u);
  CHECK(!mock_timer0_invalid_access());

  mock_timer0_clear_log();
  CHECK(!timer0_irq_pending(NULL));
  CHECK(mock_timer0_write_count() == 0u);
  CHECK(!mock_timer0_invalid_access());

  mock_timer0_set_status(&timer, UINT32_C(0xA5));
  mock_timer0_clear_log();
  CHECK(timer0_irq_pending(&timer));
  CHECK(timer.status == UINT32_C(0xA5));
  CHECK(mock_timer0_write_count() == 0u);
  CHECK(!mock_timer0_invalid_access());

  mock_timer0_clear_log();
  timer0_acknowledge_irq(&timer);
  CHECK(!timer0_irq_pending(&timer));
  CHECK(timer.status == UINT32_C(0xA4));
  CHECK(mock_timer0_write_count() == 1u);
  CHECK(check_write(
    0u,
    TIMER0_IRQ_CLEAR_OFFSET,
    TIMER0_STATUS_IRQ_PENDING
  ));

  mock_timer0_clear_log();
  timer0_acknowledge_irq(NULL);
  CHECK(mock_timer0_write_count() == 0u);
  CHECK(!mock_timer0_invalid_access());
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "standard configuration", test_standard_configuration },
    { "configuration write order", test_configuration_write_order },
    { "boundary configurations", test_boundary_configurations },
    { "invalid inputs unchanged", test_invalid_inputs_leave_registers_unchanged },
    { "reconfiguration", test_reconfiguration_replaces_programmed_values },
    { "stop behavior", test_stop_writes_only_control },
    { "interrupt query and acknowledge", test_interrupt_query_and_acknowledge },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) return 1;
    printf("ok - %s\n", tests[index].name);
  }
  return 0;
}

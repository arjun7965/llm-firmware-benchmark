#include "mock_mmio.h"

#define MOCK_TIMER0_MAX_WRITES 32u

typedef struct {
  uint32_t offset;
  uint32_t value;
} mock_write_t;

static mock_write_t writes[MOCK_TIMER0_MAX_WRITES];
static size_t write_count;
static bool invalid_access;

void mock_timer0_reset(
  volatile timer0_registers_t *timer,
  uint32_t initial_value
) {
  timer->control = initial_value;
  timer->prescaler = initial_value;
  timer->reload = initial_value;
  timer->count = initial_value;
  timer->status = initial_value;
  timer->irq_clear = initial_value;
  mock_timer0_clear_log();
}

void mock_timer0_clear_log(void) {
  write_count = 0u;
  invalid_access = false;
}

void mock_timer0_set_status(
  volatile timer0_registers_t *timer,
  uint32_t status
) {
  timer->status = status;
}

size_t mock_timer0_write_count(void) {
  return write_count;
}

uint32_t mock_timer0_write_offset(size_t index) {
  return index < write_count && index < MOCK_TIMER0_MAX_WRITES
    ? writes[index].offset
    : UINT32_MAX;
}

uint32_t mock_timer0_write_value(size_t index) {
  return index < write_count && index < MOCK_TIMER0_MAX_WRITES
    ? writes[index].value
    : UINT32_MAX;
}

bool mock_timer0_invalid_access(void) {
  return invalid_access;
}

uint32_t timer0_host_read(
  const volatile timer0_registers_t *timer,
  uint32_t offset
) {
  if (timer == NULL || offset != TIMER0_STATUS_OFFSET) {
    invalid_access = true;
    return 0u;
  }
  return timer->status;
}

void timer0_host_write(
  volatile timer0_registers_t *timer,
  uint32_t offset,
  uint32_t value
) {
  if (timer == NULL) {
    invalid_access = true;
    return;
  }
  if (write_count < MOCK_TIMER0_MAX_WRITES) {
    writes[write_count] = (mock_write_t) {
      .offset = offset,
      .value = value,
    };
  } else {
    invalid_access = true;
  }
  write_count++;

  switch (offset) {
    case TIMER0_CONTROL_OFFSET:
      timer->control = value;
      break;
    case TIMER0_PRESCALER_OFFSET:
      timer->prescaler = value;
      break;
    case TIMER0_RELOAD_OFFSET:
      timer->reload = value;
      break;
    case TIMER0_IRQ_CLEAR_OFFSET:
      timer->irq_clear = value;
      if ((value & TIMER0_STATUS_IRQ_PENDING) != 0u) {
        timer->status &= ~TIMER0_STATUS_IRQ_PENDING;
      }
      break;
    default:
      invalid_access = true;
      break;
  }
}

#include "mock_mmio_safety.h"

struct mock_mmio_event {
  enum mock_mmio_event_kind kind;
  uint32_t value;
};

struct mmio_registers {
  uint32_t status;
  uint16_t transfer_count;
  uint32_t control;
  struct mock_mmio_event events[16];
  size_t event_count;
};

static struct mmio_registers mock_registers[4];
static size_t next_register;

static void record(
  volatile mmio_registers_t *registers,
  enum mock_mmio_event_kind kind,
  uint32_t value
) {
  if (registers->event_count < 16u) {
    registers->events[registers->event_count].kind = kind;
    registers->events[registers->event_count].value = value;
    registers->event_count++;
  }
}

volatile mmio_registers_t *mock_mmio_create(void) {
  volatile mmio_registers_t *registers = &mock_registers[next_register % 4u];
  next_register++;
  mock_mmio_reset(registers);
  return registers;
}

void mock_mmio_reset(volatile mmio_registers_t *registers) {
  if (registers == NULL) return;
  registers->status = 0u;
  registers->transfer_count = 0u;
  registers->control = 0u;
  registers->event_count = 0u;
  for (size_t index = 0u; index < 16u; index++) {
    registers->events[index].kind = MOCK_MMIO_EVENT_READ_STATUS;
    registers->events[index].value = 0u;
  }
}

void mock_mmio_set_status(volatile mmio_registers_t *registers, uint32_t status) {
  registers->status = status;
}

uint32_t mock_mmio_status(const volatile mmio_registers_t *registers) {
  return registers->status;
}

uint16_t mock_mmio_transfer_count(const volatile mmio_registers_t *registers) {
  return registers->transfer_count;
}

uint32_t mock_mmio_control(const volatile mmio_registers_t *registers) {
  return registers->control;
}

size_t mock_mmio_event_count(const volatile mmio_registers_t *registers) {
  return registers->event_count;
}

bool mock_mmio_event_at(
  const volatile mmio_registers_t *registers,
  size_t index,
  enum mock_mmio_event_kind *kind_out,
  uint32_t *value_out
) {
  if (
    registers == NULL ||
    kind_out == NULL ||
    value_out == NULL ||
    index >= registers->event_count
  ) {
    return false;
  }
  *kind_out = registers->events[index].kind;
  *value_out = registers->events[index].value;
  return true;
}

uint32_t mmio_read_status(volatile mmio_registers_t *registers) {
  record(registers, MOCK_MMIO_EVENT_READ_STATUS, registers->status);
  return registers->status;
}

void mmio_write_status_clear(
  volatile mmio_registers_t *registers,
  uint32_t status
) {
  record(registers, MOCK_MMIO_EVENT_CLEAR_STATUS, status);
  registers->status &= ~status;
}

void mmio_write_transfer_count(
  volatile mmio_registers_t *registers,
  uint16_t count
) {
  record(registers, MOCK_MMIO_EVENT_WRITE_COUNT, (uint32_t)count);
  registers->transfer_count = count;
}

void mmio_write_control(
  volatile mmio_registers_t *registers,
  uint32_t control
) {
  record(registers, MOCK_MMIO_EVENT_WRITE_CONTROL, control);
  registers->control = control;
}

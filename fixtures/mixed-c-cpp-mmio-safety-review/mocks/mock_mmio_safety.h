#ifndef MOCK_MMIO_SAFETY_H
#define MOCK_MMIO_SAFETY_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "fixture_mmio_safety.h"

enum mock_mmio_event_kind {
  MOCK_MMIO_EVENT_READ_STATUS,
  MOCK_MMIO_EVENT_CLEAR_STATUS,
  MOCK_MMIO_EVENT_WRITE_COUNT,
  MOCK_MMIO_EVENT_WRITE_CONTROL,
};

#ifdef __cplusplus
extern "C" {
#endif

volatile mmio_registers_t *mock_mmio_create(void);
void mock_mmio_reset(volatile mmio_registers_t *registers);
void mock_mmio_set_status(volatile mmio_registers_t *registers, uint32_t status);
uint32_t mock_mmio_status(const volatile mmio_registers_t *registers);
uint16_t mock_mmio_transfer_count(const volatile mmio_registers_t *registers);
uint32_t mock_mmio_control(const volatile mmio_registers_t *registers);
size_t mock_mmio_event_count(const volatile mmio_registers_t *registers);
bool mock_mmio_event_at(
  const volatile mmio_registers_t *registers,
  size_t index,
  enum mock_mmio_event_kind *kind_out,
  uint32_t *value_out
);

#ifdef __cplusplus
}
#endif

#endif

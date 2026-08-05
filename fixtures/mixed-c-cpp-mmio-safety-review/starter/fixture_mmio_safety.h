#ifndef FIXTURE_MMIO_SAFETY_H
#define FIXTURE_MMIO_SAFETY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mmio_registers mmio_registers_t;

#define MMIO_STATUS_COMPLETE UINT32_C(1)
#define MMIO_STATUS_ERROR UINT32_C(2)
#define MMIO_STATUS_TERMINAL (MMIO_STATUS_COMPLETE | MMIO_STATUS_ERROR)
#define MMIO_CONTROL_DISABLED UINT32_C(0)
#define MMIO_CONTROL_START UINT32_C(1)

uint32_t mmio_read_status(volatile mmio_registers_t *registers);
void mmio_write_status_clear(
  volatile mmio_registers_t *registers,
  uint32_t status
);
void mmio_write_transfer_count(
  volatile mmio_registers_t *registers,
  uint16_t count
);
void mmio_write_control(
  volatile mmio_registers_t *registers,
  uint32_t control
);

#ifdef __cplusplus
}
#endif

#endif

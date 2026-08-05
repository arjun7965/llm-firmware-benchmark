#ifndef LEGACY_MMIO_H
#define LEGACY_MMIO_H

#include <stdint.h>

typedef struct {
  volatile uint32_t control;
  volatile uint32_t status;
  volatile uint16_t transfer_count;
} legacy_mmio_registers_t;

#endif

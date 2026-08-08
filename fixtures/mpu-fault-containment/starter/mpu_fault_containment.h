#ifndef MPU_FAULT_CONTAINMENT_H
#define MPU_FAULT_CONTAINMENT_H

#include <stdbool.h>
#include <stdint.h>

#include "fixture_mpu_fault_containment.h"

typedef struct {
  uint32_t fault_bits;
} mpu_fault_event_t;

typedef struct {
  volatile mpu0_registers_t *mpu;
  volatile security_controller_t *security;
  mpu_fault_event_t event;
  bool initialized;
  bool ready;
  bool contained;
  bool event_pending;
} mpu_fault_containment_t;

bool mpu_fault_containment_init(
  mpu_fault_containment_t *containment,
  volatile mpu0_registers_t *mpu,
  volatile security_controller_t *security
);
bool mpu_fault_containment_irq(mpu_fault_containment_t *containment);
bool mpu_fault_containment_take_event(
  mpu_fault_containment_t *containment,
  mpu_fault_event_t *event
);
bool mpu_fault_containment_ready(
  const mpu_fault_containment_t *containment
);
bool mpu_fault_containment_contained(
  const mpu_fault_containment_t *containment
);

#endif

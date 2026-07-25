#ifndef FAULT_CRASH_RECORD_H
#define FAULT_CRASH_RECORD_H

#include <stdbool.h>
#include <stdint.h>

#include "fixture_fault_crash_record.h"

#define FAULT_RECORD_MAGIC UINT32_C(0x4641554C)
#define FAULT_RECORD_CHECKSUM_XOR UINT32_C(0x91C4E27B)

typedef enum {
  FAULT_EVENT_NONE = 0,
  FAULT_EVENT_RETAINED,
  FAULT_EVENT_CAPTURED,
  FAULT_EVENT_CLEARED,
} fault_event_t;

typedef struct {
  uint32_t program_counter;
  uint32_t link_register;
  uint32_t xpsr;
} fault_frame_t;

typedef struct {
  uint32_t magic;
  uint32_t sequence;
  uint32_t status;
  uint32_t program_counter;
  uint32_t link_register;
  uint32_t xpsr;
  uint32_t checksum;
} fault_record_t;

typedef struct {
  volatile fault0_registers_t *fault;
  fault_record_t *record;
  fault_event_t event;
  bool faulted;
  bool initialized;
} fault_capture_t;

bool fault_capture_init(
  fault_capture_t *capture,
  volatile fault0_registers_t *fault,
  fault_record_t *record
);
void fault_capture_handler(
  fault_capture_t *capture,
  const fault_frame_t *frame
);
bool fault_capture_read(const fault_capture_t *capture, fault_record_t *output);
bool fault_capture_clear(fault_capture_t *capture);
fault_event_t fault_capture_take_event(fault_capture_t *capture);

#endif

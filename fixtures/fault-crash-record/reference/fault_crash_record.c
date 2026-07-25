#include <stddef.h>

#include "fault_crash_record.h"

static uint32_t record_checksum(const fault_record_t *record) {
  return record->magic ^ record->sequence ^ record->status ^
    record->program_counter ^ record->link_register ^ record->xpsr ^
    FAULT_RECORD_CHECKSUM_XOR;
}

static bool record_is_valid(const fault_record_t *record) {
  return record->magic == FAULT_RECORD_MAGIC &&
    record->checksum == record_checksum(record);
}

static void clear_record(fault_record_t *record) {
  *record = (fault_record_t) { 0 };
}

static void write_record(
  fault_record_t *record,
  uint32_t sequence,
  uint32_t status,
  const fault_frame_t *frame
) {
  *record = (fault_record_t) {
    .magic = FAULT_RECORD_MAGIC,
    .sequence = sequence,
    .status = status,
    .program_counter = frame->program_counter,
    .link_register = frame->link_register,
    .xpsr = frame->xpsr,
    .checksum = 0u,
  };
  record->checksum = record_checksum(record);
}

static bool capture_is_ready(const fault_capture_t *capture) {
  return capture != NULL && capture->initialized && capture->fault != NULL &&
    capture->record != NULL;
}

bool fault_capture_init(
  fault_capture_t *capture,
  volatile fault0_registers_t *fault,
  fault_record_t *record
) {
  const bool retained = record != NULL && record_is_valid(record);

  if (capture == NULL || fault == NULL || record == NULL) return false;

  if (!retained) clear_record(record);
  fault0_write_control(
    fault,
    retained ? FAULT0_CONTROL_SAFE : FAULT0_CONTROL_NORMAL
  );
  *capture = (fault_capture_t) {
    .fault = fault,
    .record = record,
    .event = retained ? FAULT_EVENT_RETAINED : FAULT_EVENT_NONE,
    .faulted = retained,
    .initialized = true,
  };
  return true;
}

void fault_capture_handler(
  fault_capture_t *capture,
  const fault_frame_t *frame
) {
  uint32_t sequence;
  uint32_t status;

  if (!capture_is_ready(capture) || frame == NULL) return;

  status = fault0_read_status(capture->fault) & FAULT0_STATUS_ALL;
  fault0_write_control(capture->fault, FAULT0_CONTROL_SAFE);
  sequence = record_is_valid(capture->record)
    ? capture->record->sequence + UINT32_C(1)
    : UINT32_C(1);
  write_record(capture->record, sequence, status, frame);
  if (status != 0u) {
    fault0_write_status_clear(capture->fault, status);
  }
  capture->faulted = true;
  capture->event = FAULT_EVENT_CAPTURED;
}

bool fault_capture_read(const fault_capture_t *capture, fault_record_t *output) {
  uint32_t irq_state;

  if (!capture_is_ready(capture) || output == NULL) return false;

  irq_state = fault0_irq_save_disable();
  if (!capture->faulted || !record_is_valid(capture->record)) {
    fault0_irq_restore(irq_state);
    return false;
  }
  *output = *capture->record;
  fault0_irq_restore(irq_state);
  return true;
}

bool fault_capture_clear(fault_capture_t *capture) {
  uint32_t irq_state;

  if (!capture_is_ready(capture)) return false;

  irq_state = fault0_irq_save_disable();
  if (
    !capture->faulted || capture->event != FAULT_EVENT_NONE ||
    !record_is_valid(capture->record)
  ) {
    fault0_irq_restore(irq_state);
    return false;
  }
  clear_record(capture->record);
  fault0_write_control(capture->fault, FAULT0_CONTROL_NORMAL);
  capture->faulted = false;
  capture->event = FAULT_EVENT_CLEARED;
  fault0_irq_restore(irq_state);
  return true;
}

fault_event_t fault_capture_take_event(fault_capture_t *capture) {
  fault_event_t event;
  uint32_t irq_state;

  if (!capture_is_ready(capture)) return FAULT_EVENT_NONE;

  irq_state = fault0_irq_save_disable();
  event = capture->event;
  capture->event = FAULT_EVENT_NONE;
  fault0_irq_restore(irq_state);
  return event;
}

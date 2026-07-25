#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "fault_crash_record.h"
#include "mock_fault_crash_record.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
        __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

typedef struct {
  mock_fault_event_t event;
  uint32_t value;
} expected_event_t;

static uint32_t record_checksum(const fault_record_t *record) {
  return record->magic ^ record->sequence ^ record->status ^
    record->program_counter ^ record->link_register ^ record->xpsr ^
    FAULT_RECORD_CHECKSUM_XOR;
}

static bool record_is_valid(const fault_record_t *record) {
  return record->magic == FAULT_RECORD_MAGIC &&
    record->checksum == record_checksum(record);
}

static bool capture_equals(
  const fault_capture_t *left,
  const fault_capture_t *right
) {
  return left->fault == right->fault && left->record == right->record &&
    left->event == right->event && left->faulted == right->faulted &&
    left->initialized == right->initialized;
}

static bool events_match_from(
  size_t offset,
  const expected_event_t *expected,
  size_t expected_count
) {
  if (mock_fault0_event_count() != offset + expected_count) return false;
  for (size_t index = 0u; index < expected_count; index++) {
    if (
      mock_fault0_event_at(offset + index) != expected[index].event ||
      mock_fault0_event_value(offset + index) != expected[index].value
    ) {
      return false;
    }
  }
  return true;
}

static bool initialize(fault_capture_t *capture, fault_record_t *record) {
  return fault_capture_init(capture, mock_fault0(), record);
}

static bool test_validation_and_corrupt_record_boot(void) {
  fault_capture_t capture = {
    .fault = (volatile fault0_registers_t *)(uintptr_t)UINT32_C(1),
    .record = (fault_record_t *)(uintptr_t)UINT32_C(1),
    .event = FAULT_EVENT_CAPTURED,
    .faulted = true,
    .initialized = true,
  };
  fault_record_t record = {
    .magic = FAULT_RECORD_MAGIC,
    .sequence = UINT32_C(3),
    .status = FAULT0_STATUS_HARDFAULT,
    .program_counter = UINT32_C(0x101),
    .link_register = UINT32_C(0x201),
    .xpsr = UINT32_C(0x01000000),
    .checksum = 0u,
  };
  const fault_capture_t before_capture = capture;
  const fault_record_t before_record = record;
  const expected_event_t expected[] = {
    { MOCK_FAULT_EVENT_CONTROL_WRITE, FAULT0_CONTROL_NORMAL },
  };

  mock_fault0_reset();
  CHECK(!fault_capture_init(NULL, mock_fault0(), &record));
  CHECK(!fault_capture_init(&capture, NULL, &record));
  CHECK(!fault_capture_init(&capture, mock_fault0(), NULL));
  CHECK(capture_equals(&capture, &before_capture));
  CHECK(record.magic == before_record.magic);
  CHECK(record.sequence == before_record.sequence);
  CHECK(record.checksum == before_record.checksum);
  CHECK(mock_fault0_event_count() == 0u);

  CHECK(initialize(&capture, &record));
  CHECK(events_match_from(0u, expected, sizeof(expected) / sizeof(expected[0])));
  CHECK(record.magic == 0u);
  CHECK(record.sequence == 0u);
  CHECK(!capture.faulted);
  CHECK(capture.event == FAULT_EVENT_NONE);
  CHECK(mock_fault0_control() == FAULT0_CONTROL_NORMAL);
  CHECK(!mock_fault0_invalid_access());
  return true;
}

static bool test_retained_record_safe_boot_and_clear_gating(void) {
  fault_capture_t capture = { 0 };
  fault_record_t record = {
    .magic = FAULT_RECORD_MAGIC,
    .sequence = UINT32_C(7),
    .status = FAULT0_STATUS_BUSFAULT,
    .program_counter = UINT32_C(0x101),
    .link_register = UINT32_C(0x201),
    .xpsr = UINT32_C(0x01000000),
    .checksum = 0u,
  };
  fault_record_t output = { 0 };
  size_t offset;

  record.checksum = record_checksum(&record);
  mock_fault0_reset();
  CHECK(initialize(&capture, &record));
  CHECK(mock_fault0_control() == FAULT0_CONTROL_SAFE);
  CHECK(capture.faulted);
  CHECK(capture.event == FAULT_EVENT_RETAINED);
  mock_fault0_set_irq_state(UINT32_C(0xA5));

  offset = mock_fault0_event_count();
  CHECK(fault_capture_read(&capture, &output));
  CHECK(events_match_from(offset, (const expected_event_t[]) {
    { MOCK_FAULT_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xA5) },
    { MOCK_FAULT_EVENT_IRQ_RESTORE, UINT32_C(0xA5) },
  }, 2u));
  CHECK(output.sequence == UINT32_C(7));
  CHECK(output.program_counter == UINT32_C(0x101));

  offset = mock_fault0_event_count();
  CHECK(!fault_capture_clear(&capture));
  CHECK(events_match_from(offset, (const expected_event_t[]) {
    { MOCK_FAULT_EVENT_IRQ_SAVE_DISABLE, UINT32_C(0xA5) },
    { MOCK_FAULT_EVENT_IRQ_RESTORE, UINT32_C(0xA5) },
  }, 2u));
  CHECK(fault_capture_take_event(&capture) == FAULT_EVENT_RETAINED);
  CHECK(fault_capture_clear(&capture));
  CHECK(mock_fault0_control() == FAULT0_CONTROL_NORMAL);
  CHECK(record.magic == 0u);
  CHECK(!capture.faulted);
  CHECK(capture.event == FAULT_EVENT_CLEARED);
  CHECK(fault_capture_take_event(&capture) == FAULT_EVENT_CLEARED);
  CHECK(!mock_fault0_invalid_access());
  return true;
}

static bool test_fault_handler_capture_and_repeated_fault_sequence(void) {
  fault_capture_t capture = { 0 };
  fault_record_t record = { 0 };
  fault_record_t output = { 0 };
  const fault_frame_t first = {
    .program_counter = UINT32_C(0x1001),
    .link_register = UINT32_C(0x2001),
    .xpsr = UINT32_C(0x01000000),
  };
  const fault_frame_t second = {
    .program_counter = UINT32_C(0x3001),
    .link_register = UINT32_C(0x4001),
    .xpsr = UINT32_C(0x21000000),
  };
  const expected_event_t handler_events[] = {
    { MOCK_FAULT_EVENT_STATUS_READ, FAULT0_STATUS_HARDFAULT | FAULT0_STATUS_BUSFAULT },
    { MOCK_FAULT_EVENT_CONTROL_WRITE, FAULT0_CONTROL_SAFE },
    { MOCK_FAULT_EVENT_STATUS_CLEAR_WRITE, FAULT0_STATUS_HARDFAULT | FAULT0_STATUS_BUSFAULT },
  };
  size_t offset;

  mock_fault0_reset();
  CHECK(initialize(&capture, &record));
  mock_fault0_set_status(FAULT0_STATUS_HARDFAULT | FAULT0_STATUS_BUSFAULT);
  offset = mock_fault0_event_count();
  fault_capture_handler(&capture, &first);
  CHECK(events_match_from(
    offset,
    handler_events,
    sizeof(handler_events) / sizeof(handler_events[0])
  ));
  CHECK(mock_fault0_control() == FAULT0_CONTROL_SAFE);
  CHECK(mock_fault0_status() == 0u);
  CHECK(capture.faulted);
  CHECK(capture.event == FAULT_EVENT_CAPTURED);
  CHECK(record_is_valid(&record));
  CHECK(record.sequence == UINT32_C(1));
  CHECK(record.status == (FAULT0_STATUS_HARDFAULT | FAULT0_STATUS_BUSFAULT));
  CHECK(record.program_counter == first.program_counter);
  CHECK(record.link_register == first.link_register);

  CHECK(fault_capture_take_event(&capture) == FAULT_EVENT_CAPTURED);
  mock_fault0_set_status(FAULT0_STATUS_HARDFAULT);
  fault_capture_handler(&capture, &second);
  CHECK(record_is_valid(&record));
  CHECK(record.sequence == UINT32_C(2));
  CHECK(record.status == FAULT0_STATUS_HARDFAULT);
  CHECK(record.program_counter == second.program_counter);
  CHECK(record.link_register == second.link_register);
  CHECK(fault_capture_read(&capture, &output));
  CHECK(output.checksum == record.checksum);
  CHECK(fault_capture_take_event(&capture) == FAULT_EVENT_CAPTURED);
  CHECK(fault_capture_clear(&capture));
  CHECK(!mock_fault0_invalid_access());
  return true;
}

static bool test_invalid_foreground_and_handler_calls_have_no_side_effects(void) {
  fault_capture_t capture = { 0 };
  fault_record_t record = { 0 };
  fault_record_t output = { 0 };
  const fault_capture_t before_capture = capture;
  const fault_record_t before_record = record;
  const fault_frame_t frame = { 0 };

  mock_fault0_reset();
  fault_capture_handler(NULL, &frame);
  fault_capture_handler(&capture, NULL);
  fault_capture_handler(&capture, &frame);
  CHECK(!fault_capture_read(NULL, &output));
  CHECK(!fault_capture_read(&capture, &output));
  CHECK(!fault_capture_read(&capture, NULL));
  CHECK(!fault_capture_clear(NULL));
  CHECK(!fault_capture_clear(&capture));
  CHECK(fault_capture_take_event(NULL) == FAULT_EVENT_NONE);
  CHECK(fault_capture_take_event(&capture) == FAULT_EVENT_NONE);
  CHECK(capture_equals(&capture, &before_capture));
  CHECK(record.magic == before_record.magic);
  CHECK(record.checksum == before_record.checksum);
  CHECK(mock_fault0_event_count() == 0u);
  CHECK(!mock_fault0_invalid_access());
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "validation and corrupt boot", test_validation_and_corrupt_record_boot },
    { "retained record safe boot", test_retained_record_safe_boot_and_clear_gating },
    { "fault handler capture", test_fault_handler_capture_and_repeated_fault_sequence },
    { "invalid calls", test_invalid_foreground_and_handler_calls_have_no_side_effects },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) {
      fprintf(stderr, "failed: %s\n", tests[index].name);
      return 1;
    }
  }

  printf("Fault crash-record public tests passed (%zu tests).\n",
    sizeof(tests) / sizeof(tests[0]));
  return 0;
}

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


static bool record_equals(const fault_record_t *a, const fault_record_t *b) {
  return a->magic == b->magic && a->sequence == b->sequence &&
    a->status == b->status && a->program_counter == b->program_counter &&
    a->link_register == b->link_register && a->xpsr == b->xpsr &&
    a->checksum == b->checksum;
}

static fault_record_t valid_record(uint32_t sequence) {
  fault_record_t record = {
    .magic = FAULT_RECORD_MAGIC,
    .sequence = sequence,
    .status = FAULT0_STATUS_BUSFAULT,
    .program_counter = UINT32_C(0x1001),
    .link_register = UINT32_C(0x2001),
    .xpsr = UINT32_C(0x21000000),
  };
  record.checksum = record_checksum(&record);
  return record;
}

static bool irq_pair_from(size_t offset, uint32_t state) {
  return events_match_from(offset, (const expected_event_t[]) {
    { MOCK_FAULT_EVENT_IRQ_SAVE_DISABLE, state },
    { MOCK_FAULT_EVENT_IRQ_RESTORE, state },
  }, 2u) && mock_fault0_irq_state() == state;
}

static bool test_isolated_corruption_and_repeated_boot(void) {
  const fault_record_t zero = { 0 };
  for (unsigned field = 0u; field < 8u; field++) {
    fault_record_t record = valid_record(UINT32_C(9));
    fault_capture_t capture = { 0 };
    switch (field) {
      case 0: record.magic ^= UINT32_C(1); break;
      case 1: record.sequence ^= UINT32_C(1); break;
      case 2: record.status ^= UINT32_C(1); break;
      case 3: record.program_counter ^= UINT32_C(1); break;
      case 4: record.link_register ^= UINT32_C(1); break;
      case 5: record.xpsr ^= UINT32_C(1); break;
      case 6: record.checksum ^= UINT32_C(1); break;
      default:
        record.magic ^= UINT32_C(1);
        record.checksum = record_checksum(&record);
        break;
    }
    mock_fault0_reset();
    mock_fault0_watch_record(&record);
    CHECK(initialize(&capture, &record));
    CHECK(record_equals(&record, &zero));
    CHECK(events_match_from(0u, (const expected_event_t[]) {
      { MOCK_FAULT_EVENT_CONTROL_WRITE, FAULT0_CONTROL_NORMAL },
    }, 1u));
    const fault_record_t at_control = mock_fault0_record_at(0u);
    CHECK(record_equals(&at_control, &zero));
    CHECK(!capture.faulted && capture.event == FAULT_EVENT_NONE);
    CHECK(capture.initialized && capture.record == &record);
    CHECK(capture.fault == mock_fault0());
    CHECK(!mock_fault0_invalid_access());
  }
  fault_record_t record = valid_record(UINT32_MAX);
  record.status = UINT32_MAX;
  record.checksum = record_checksum(&record);
  const fault_record_t before = record;
  for (unsigned boot = 0u; boot < 3u; boot++) {
    fault_capture_t capture = { 0 };
    mock_fault0_reset();
    mock_fault0_watch_record(&record);
    CHECK(initialize(&capture, &record));
    CHECK(record_equals(&record, &before));
    CHECK(capture.faulted && capture.event == FAULT_EVENT_RETAINED);
    CHECK(events_match_from(0u, (const expected_event_t[]) {
      { MOCK_FAULT_EVENT_CONTROL_WRITE, FAULT0_CONTROL_SAFE },
    }, 1u));
    CHECK(!mock_fault0_invalid_access());
  }
  return true;
}

static bool test_handler_zero_reserved_and_single_status(void) {
  const uint32_t statuses[] = {
    0u, UINT32_C(0x80000000), FAULT0_STATUS_HARDFAULT,
    FAULT0_STATUS_BUSFAULT | UINT32_C(0x80000000), FAULT0_STATUS_ALL,
  };
  const fault_frame_t frame = {
    .program_counter = UINT32_C(0x12345679),
    .link_register = UINT32_C(0xFFFFFFFD),
    .xpsr = UINT32_C(0x61000000),
  };
  for (size_t i = 0u; i < sizeof(statuses) / sizeof(statuses[0]); i++) {
    fault_capture_t capture = { 0 };
    fault_record_t record = { 0 };
    mock_fault0_reset();
    CHECK(initialize(&capture, &record));
    mock_fault0_watch_record(&record);
    mock_fault0_set_status(statuses[i]);
    mock_fault0_set_irq_state(UINT32_C(0xA5));
    const fault_record_t before = record;
    const size_t offset = mock_fault0_event_count();
    const uint32_t captured = statuses[i] & FAULT0_STATUS_ALL;
    fault_capture_handler(&capture, &frame);
    const expected_event_t events[] = {
      { MOCK_FAULT_EVENT_STATUS_READ, statuses[i] },
      { MOCK_FAULT_EVENT_CONTROL_WRITE, FAULT0_CONTROL_SAFE },
      { MOCK_FAULT_EVENT_STATUS_CLEAR_WRITE, captured },
    };
    CHECK(events_match_from(offset, events, captured == 0u ? 2u : 3u));
    const fault_record_t at_safe = mock_fault0_record_at(offset + 1u);
    CHECK(record_equals(&at_safe, &before));
    CHECK(mock_fault0_irq_state() == UINT32_C(0xA5));
    CHECK(mock_fault0_control() == FAULT0_CONTROL_SAFE);
    CHECK(mock_fault0_status() == (statuses[i] & ~FAULT0_STATUS_ALL));
    CHECK(record_is_valid(&record) && record.sequence == UINT32_C(1));
    CHECK(record.status == captured);
    CHECK(record.program_counter == frame.program_counter);
    CHECK(record.link_register == frame.link_register);
    CHECK(record.xpsr == frame.xpsr);
    if (captured != 0u) {
      const fault_record_t at_clear = mock_fault0_record_at(offset + 2u);
      CHECK(record_equals(&at_clear, &record));
    }
    CHECK(capture.faulted && capture.event == FAULT_EVENT_CAPTURED);
    CHECK(!mock_fault0_invalid_access());
  }
  return true;
}

static bool test_sequence_wrap_corruption_and_event_replacement(void) {
  fault_capture_t capture = { 0 };
  fault_record_t record = valid_record(UINT32_MAX);
  const fault_frame_t frame = {
    .program_counter = UINT32_C(0x3001),
    .link_register = UINT32_C(0x4001),
    .xpsr = UINT32_C(0x01000000),
  };
  mock_fault0_reset();
  CHECK(initialize(&capture, &record));
  /* A new fault replaces even an unconsumed retained event. */
  fault_capture_handler(&capture, &frame);
  CHECK(record_is_valid(&record) && record.sequence == 0u);
  CHECK(fault_capture_take_event(&capture) == FAULT_EVENT_CAPTURED);
  record.sequence = UINT32_C(17);
  record.checksum ^= UINT32_C(1);
  fault_capture_handler(&capture, &frame);
  CHECK(record_is_valid(&record) && record.sequence == UINT32_C(1));
  fault_capture_handler(&capture, &frame);
  CHECK(record_is_valid(&record) && record.sequence == UINT32_C(2));
  CHECK(fault_capture_take_event(&capture) == FAULT_EVENT_CAPTURED);
  CHECK(fault_capture_clear(&capture));
  CHECK(capture.event == FAULT_EVENT_CLEARED);
  fault_capture_handler(&capture, &frame);
  CHECK(record_is_valid(&record) && record.sequence == UINT32_C(1));
  CHECK(capture.faulted && capture.event == FAULT_EVENT_CAPTURED);
  CHECK(fault_capture_take_event(&capture) == FAULT_EVENT_CAPTURED);
  CHECK(fault_capture_take_event(&capture) == FAULT_EVENT_NONE);
  CHECK(!mock_fault0_invalid_access());
  return true;
}

static bool test_foreground_failure_preserves_state_and_output(void) {
  const uint32_t states[] = { 0u, UINT32_C(1), UINT32_C(0xA5) };
  for (size_t i = 0u; i < sizeof(states) / sizeof(states[0]); i++) {
    for (unsigned scenario = 0u; scenario < 4u; scenario++) {
      fault_capture_t capture = { 0 };
      fault_record_t record = scenario == 0u
        ? (fault_record_t) { 0 } : valid_record(UINT32_C(7));
      fault_record_t output = {
        .magic = UINT32_MAX,
        .sequence = UINT32_MAX - UINT32_C(1),
        .status = UINT32_C(0xAAAAAAAA),
        .program_counter = UINT32_C(0x99999999),
        .link_register = UINT32_C(0xBBBBBBBB),
        .xpsr = UINT32_C(0xCCCCCCCC),
        .checksum = UINT32_C(0xDDDDDDDD),
      };
      const fault_record_t before_output = output;
      mock_fault0_reset();
      CHECK(initialize(&capture, &record));
      if (scenario == 2u) record.checksum ^= UINT32_C(1);
      if (scenario == 3u) capture.faulted = false;
      const fault_record_t before_record = record;
      const fault_capture_t before_capture = capture;
      mock_fault0_set_irq_state(states[i]);
      size_t offset = mock_fault0_event_count();
      const bool read_ok = fault_capture_read(&capture, &output);
      CHECK(read_ok == (scenario == 1u));
      CHECK(irq_pair_from(offset, states[i]));
      CHECK(record_equals(&output, read_ok ? &record : &before_output));
      CHECK(capture_equals(&capture, &before_capture));
      offset = mock_fault0_event_count();
      CHECK(!fault_capture_clear(&capture));
      CHECK(irq_pair_from(offset, states[i]));
      CHECK(record_equals(&record, &before_record));
      CHECK(capture_equals(&capture, &before_capture));
      offset = mock_fault0_event_count();
      CHECK(fault_capture_take_event(&capture) == before_capture.event);
      CHECK(irq_pair_from(offset, states[i]));
      if (scenario != 1u) {
        const fault_capture_t after_take = capture;
        offset = mock_fault0_event_count();
        CHECK(!fault_capture_clear(&capture));
        CHECK(irq_pair_from(offset, states[i]));
        CHECK(record_equals(&record, &before_record));
        CHECK(capture_equals(&capture, &after_take));
      }
      CHECK(!mock_fault0_invalid_access());
    }
  }
  return true;
}

static bool test_complete_clear_trace_and_reboot(void) {
  const uint32_t states[] = { 0u, UINT32_C(1), UINT32_C(0xA5) };
  const fault_record_t zero = { 0 };
  for (size_t i = 0u; i < sizeof(states) / sizeof(states[0]); i++) {
    fault_capture_t capture = { 0 };
    fault_record_t record = valid_record(UINT32_C(8));
    mock_fault0_reset();
    CHECK(initialize(&capture, &record));
    mock_fault0_set_irq_state(states[i]);
    CHECK(fault_capture_take_event(&capture) == FAULT_EVENT_RETAINED);
    mock_fault0_watch_record(&record);
    size_t offset = mock_fault0_event_count();
    CHECK(fault_capture_clear(&capture));
    CHECK(events_match_from(offset, (const expected_event_t[]) {
      { MOCK_FAULT_EVENT_IRQ_SAVE_DISABLE, states[i] },
      { MOCK_FAULT_EVENT_CONTROL_WRITE, FAULT0_CONTROL_NORMAL },
      { MOCK_FAULT_EVENT_IRQ_RESTORE, states[i] },
    }, 3u));
    const fault_record_t at_normal = mock_fault0_record_at(offset + 1u);
    CHECK(record_equals(&at_normal, &zero));
    CHECK(record_equals(&record, &zero));
    CHECK(mock_fault0_irq_state() == states[i]);
    CHECK(!capture.faulted && capture.event == FAULT_EVENT_CLEARED);
    offset = mock_fault0_event_count();
    CHECK(!fault_capture_clear(&capture));
    CHECK(irq_pair_from(offset, states[i]));
    CHECK(capture.event == FAULT_EVENT_CLEARED);
    CHECK(fault_capture_take_event(&capture) == FAULT_EVENT_CLEARED);
    offset = mock_fault0_event_count();
    CHECK(fault_capture_take_event(&capture) == FAULT_EVENT_NONE);
    CHECK(irq_pair_from(offset, states[i]));
    CHECK(!mock_fault0_invalid_access());
    mock_fault0_reset();
    CHECK(initialize(&capture, &record));
    CHECK(!capture.faulted && capture.event == FAULT_EVENT_NONE);
    CHECK(mock_fault0_control() == FAULT0_CONTROL_NORMAL);
  }
  return true;
}

static bool test_partially_initialized_and_null_frame(void) {
  fault_record_t record = valid_record(UINT32_C(5));
  fault_record_t output = valid_record(UINT32_C(17));
  const fault_record_t before_record = record;
  const fault_record_t before_output = output;
  const fault_frame_t frame = { 0 };
  for (unsigned scenario = 0u; scenario < 4u; scenario++) {
    fault_capture_t capture = {
      .fault = mock_fault0(), .record = &record,
      .initialized = true, .faulted = true, .event = FAULT_EVENT_RETAINED,
    };
    if (scenario == 0u) capture.initialized = false;
    if (scenario == 1u) capture.fault = NULL;
    if (scenario == 2u) capture.record = NULL;
    const fault_capture_t before = capture;
    mock_fault0_reset();
    fault_capture_handler(&capture, scenario == 3u ? NULL : &frame);
    CHECK(capture_equals(&capture, &before));
    CHECK(record_equals(&record, &before_record));
    CHECK(mock_fault0_event_count() == 0u);
    if (scenario != 3u) {
      CHECK(!fault_capture_read(&capture, &output));
      CHECK(!fault_capture_clear(&capture));
      CHECK(fault_capture_take_event(&capture) == FAULT_EVENT_NONE);
    } else {
      CHECK(!fault_capture_read(&capture, NULL));
    }
    CHECK(record_equals(&output, &before_output));
    CHECK(record_equals(&record, &before_record));
    CHECK(capture_equals(&capture, &before));
    CHECK(mock_fault0_event_count() == 0u);
    CHECK(!mock_fault0_invalid_access());
  }
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
    { "isolated corruption and reboot", test_isolated_corruption_and_repeated_boot },
    { "zero and reserved status", test_handler_zero_reserved_and_single_status },
    { "sequence and replacement", test_sequence_wrap_corruption_and_event_replacement },
    { "foreground failure", test_foreground_failure_preserves_state_and_output },
    { "clear trace and reboot", test_complete_clear_trace_and_reboot },
    { "partially initialized calls", test_partially_initialized_and_null_frame },
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

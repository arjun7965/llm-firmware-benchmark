#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "i2c_controller.h"
#include "mock_i2c_controller.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
              __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

typedef struct {
  mock_i2c_event_t event;
  uint32_t value;
} expected_event_t;

static bool initialize(i2c_controller_t *controller) {
  return i2c_controller_init(controller, mock_i2c0());
}

static bool state_equals(
  const i2c_controller_t *left,
  const i2c_controller_t *right
) {
  return left->i2c == right->i2c && left->data == right->data &&
    left->length == right->length && left->next_index == right->next_index &&
    left->address == right->address &&
    left->started_at_ms == right->started_at_ms &&
    left->phase == right->phase && left->result == right->result &&
    left->busy == right->busy && left->initialized == right->initialized;
}

static bool transaction_is_clear(const i2c_controller_t *controller) {
  return controller->data == NULL && controller->length == 0u &&
    controller->next_index == 0u && controller->address == 0u &&
    controller->started_at_ms == 0u &&
    controller->phase == I2C_CONTROLLER_PHASE_IDLE && !controller->busy;
}

static bool events_match_from(
  size_t offset,
  const expected_event_t *expected,
  size_t expected_count
) {
  if (mock_i2c_event_count() != offset + expected_count) return false;
  for (size_t index = 0u; index < expected_count; index++) {
    if (mock_i2c_event_at(offset + index) != expected[index].event ||
      mock_i2c_event_value(offset + index) != expected[index].value) {
      return false;
    }
  }
  return true;
}

static bool test_initialization_validation_and_recovery(void) {
  i2c_controller_t controller = {
    .i2c = (volatile i2c0_registers_t *)(uintptr_t)UINT32_C(1),
    .data = (const uint8_t *)(uintptr_t)UINT32_C(2),
    .length = 3u,
    .next_index = 2u,
    .address = UINT8_C(0x48),
    .started_at_ms = 99u,
    .phase = I2C_CONTROLLER_PHASE_WAIT_DATA,
    .result = I2C_CONTROLLER_RESULT_NACK,
    .busy = true,
    .initialized = true,
  };
  const i2c_controller_t before = controller;
  const uint8_t data[] = { UINT8_C(0x10) };
  const expected_event_t initialization_events[] = {
    { MOCK_I2C_EVENT_CONTROL_WRITE, 0u },
    { MOCK_I2C_EVENT_STATUS_CLEAR, I2C0_STATUS_ALL },
    { MOCK_I2C_EVENT_CONTROL_WRITE, I2C0_CONTROL_ENABLE },
  };

  mock_i2c_reset();
  CHECK(!i2c_controller_init(NULL, mock_i2c0()));
  CHECK(!i2c_controller_init(&controller, NULL));
  CHECK(state_equals(&controller, &before));
  CHECK(mock_i2c_event_count() == 0u);

  CHECK(initialize(&controller));
  CHECK(controller.i2c == mock_i2c0());
  CHECK(transaction_is_clear(&controller));
  CHECK(controller.result == I2C_CONTROLLER_RESULT_NONE);
  CHECK(controller.initialized);
  CHECK(events_match_from(
    0u,
    initialization_events,
    sizeof(initialization_events) / sizeof(initialization_events[0])
  ));
  CHECK(!mock_i2c_invalid_access());

  CHECK(i2c_controller_start_write(&controller, UINT8_C(0x48), data, 1u, 1u));
  CHECK(initialize(&controller));
  CHECK(transaction_is_clear(&controller));
  CHECK(controller.result == I2C_CONTROLLER_RESULT_NONE);
  CHECK(controller.initialized);
  CHECK(mock_i2c_control() == I2C0_CONTROL_ENABLE);
  CHECK(mock_i2c_status() == 0u);
  CHECK(!mock_i2c_invalid_access());
  return true;
}

static bool test_start_validation_stale_status_and_busy_rejection(void) {
  i2c_controller_t controller = { 0 };
  i2c_controller_t uninitialized = { 0 };
  const uint8_t data[] = {
    UINT8_C(0x11), UINT8_C(0x22), UINT8_C(0x33), UINT8_C(0x44),
  };
  i2c_controller_t before;
  size_t offset;
  const expected_event_t start_events[] = {
    { MOCK_I2C_EVENT_STATUS_CLEAR, I2C0_STATUS_ALL },
    {
      MOCK_I2C_EVENT_CONTROL_WRITE,
      I2C0_CONTROL_ENABLE | I2C0_CONTROL_START,
    },
  };

  mock_i2c_reset();
  CHECK(!i2c_controller_start_write(
    &uninitialized,
    UINT8_C(0x48),
    data,
    1u,
    0u
  ));
  CHECK(mock_i2c_event_count() == 0u);
  CHECK(initialize(&controller));
  before = controller;
  CHECK(!i2c_controller_start_write(
    &controller,
    I2C_CONTROLLER_MIN_ADDRESS - UINT8_C(1),
    data,
    1u,
    0u
  ));
  CHECK(!i2c_controller_start_write(
    &controller,
    I2C_CONTROLLER_MAX_ADDRESS + UINT8_C(1),
    data,
    1u,
    0u
  ));
  CHECK(!i2c_controller_start_write(
    &controller,
    UINT8_C(0x48),
    NULL,
    1u,
    0u
  ));
  CHECK(!i2c_controller_start_write(
    &controller,
    UINT8_C(0x48),
    data,
    0u,
    0u
  ));
  CHECK(!i2c_controller_start_write(
    &controller,
    UINT8_C(0x48),
    data,
    I2C_CONTROLLER_MAX_WRITE_BYTES + 1u,
    0u
  ));
  CHECK(state_equals(&controller, &before));

  mock_i2c_set_status(I2C0_STATUS_ALL);
  offset = mock_i2c_event_count();
  CHECK(i2c_controller_start_write(
    &controller,
    UINT8_C(0x48),
    data,
    sizeof(data),
    123u
  ));
  CHECK(events_match_from(
    offset,
    start_events,
    sizeof(start_events) / sizeof(start_events[0])
  ));
  CHECK(mock_i2c_status() == 0u);
  CHECK(controller.data == data);
  CHECK(controller.length == sizeof(data));
  CHECK(controller.next_index == 0u);
  CHECK(controller.address == UINT8_C(0x48));
  CHECK(controller.started_at_ms == 123u);
  CHECK(controller.phase == I2C_CONTROLLER_PHASE_WAIT_START);
  CHECK(controller.busy);
  before = controller;
  CHECK(!i2c_controller_start_write(
    &controller,
    UINT8_C(0x49),
    data,
    1u,
    124u
  ));
  CHECK(state_equals(&controller, &before));
  CHECK(i2c_controller_take_result(&controller) == I2C_CONTROLLER_RESULT_NONE);
  CHECK(!mock_i2c_invalid_access());
  return true;
}

static bool test_write_protocol_completion_and_result_consumption(void) {
  i2c_controller_t controller = { 0 };
  const uint8_t data[] = {
    UINT8_C(0xA1), UINT8_C(0xB2), UINT8_C(0xC3),
  };
  size_t offset;
  const expected_event_t start_events[] = {
    { MOCK_I2C_EVENT_STATUS_READ, I2C0_STATUS_START },
    { MOCK_I2C_EVENT_STATUS_CLEAR, I2C0_STATUS_START },
    { MOCK_I2C_EVENT_DATA_WRITE, UINT8_C(0x90) },
    { MOCK_I2C_EVENT_CONTROL_WRITE, I2C0_CONTROL_ENABLE },
  };
  const expected_event_t address_events[] = {
    { MOCK_I2C_EVENT_STATUS_READ, I2C0_STATUS_ADDRESS_ACK },
    { MOCK_I2C_EVENT_STATUS_CLEAR, I2C0_STATUS_ADDRESS_ACK },
    { MOCK_I2C_EVENT_DATA_WRITE, UINT8_C(0xA1) },
    { MOCK_I2C_EVENT_CONTROL_WRITE, I2C0_CONTROL_ENABLE },
  };
  const expected_event_t data_events[] = {
    { MOCK_I2C_EVENT_STATUS_READ, I2C0_STATUS_DATA_ACK },
    { MOCK_I2C_EVENT_STATUS_CLEAR, I2C0_STATUS_DATA_ACK },
    { MOCK_I2C_EVENT_DATA_WRITE, UINT8_C(0xB2) },
    { MOCK_I2C_EVENT_CONTROL_WRITE, I2C0_CONTROL_ENABLE },
  };
  const expected_event_t final_data_events[] = {
    { MOCK_I2C_EVENT_STATUS_READ, I2C0_STATUS_DATA_ACK },
    { MOCK_I2C_EVENT_STATUS_CLEAR, I2C0_STATUS_DATA_ACK },
    { MOCK_I2C_EVENT_DATA_WRITE, UINT8_C(0xC3) },
    { MOCK_I2C_EVENT_CONTROL_WRITE, I2C0_CONTROL_ENABLE },
  };
  const expected_event_t completion_events[] = {
    { MOCK_I2C_EVENT_STATUS_READ, I2C0_STATUS_DATA_ACK },
    { MOCK_I2C_EVENT_STATUS_CLEAR, I2C0_STATUS_DATA_ACK },
    {
      MOCK_I2C_EVENT_CONTROL_WRITE,
      I2C0_CONTROL_ENABLE | I2C0_CONTROL_STOP,
    },
  };

  mock_i2c_reset();
  CHECK(initialize(&controller));
  CHECK(i2c_controller_start_write(
    &controller,
    UINT8_C(0x48),
    data,
    sizeof(data),
    100u
  ));

  offset = mock_i2c_event_count();
  CHECK(mock_i2c_signal_start());
  CHECK(i2c_controller_poll(&controller, 101u) == I2C_CONTROLLER_RESULT_NONE);
  CHECK(events_match_from(
    offset,
    start_events,
    sizeof(start_events) / sizeof(start_events[0])
  ));
  CHECK(controller.phase == I2C_CONTROLLER_PHASE_WAIT_ADDRESS);
  CHECK(controller.next_index == 0u);

  offset = mock_i2c_event_count();
  CHECK(mock_i2c_signal_address_ack());
  CHECK(i2c_controller_poll(&controller, 102u) == I2C_CONTROLLER_RESULT_NONE);
  CHECK(events_match_from(
    offset,
    address_events,
    sizeof(address_events) / sizeof(address_events[0])
  ));
  CHECK(controller.phase == I2C_CONTROLLER_PHASE_WAIT_DATA);
  CHECK(controller.next_index == 1u);

  offset = mock_i2c_event_count();
  CHECK(mock_i2c_signal_data_ack());
  CHECK(i2c_controller_poll(&controller, 103u) == I2C_CONTROLLER_RESULT_NONE);
  CHECK(events_match_from(
    offset,
    data_events,
    sizeof(data_events) / sizeof(data_events[0])
  ));
  CHECK(controller.next_index == 2u);

  offset = mock_i2c_event_count();
  CHECK(mock_i2c_signal_data_ack());
  CHECK(i2c_controller_poll(&controller, 104u) == I2C_CONTROLLER_RESULT_NONE);
  CHECK(events_match_from(
    offset,
    final_data_events,
    sizeof(final_data_events) / sizeof(final_data_events[0])
  ));
  CHECK(controller.next_index == 3u);

  offset = mock_i2c_event_count();
  CHECK(mock_i2c_signal_data_ack());
  CHECK(i2c_controller_poll(&controller, 105u) ==
    I2C_CONTROLLER_RESULT_COMPLETE);
  CHECK(events_match_from(
    offset,
    completion_events,
    sizeof(completion_events) / sizeof(completion_events[0])
  ));
  CHECK(transaction_is_clear(&controller));
  CHECK(controller.result == I2C_CONTROLLER_RESULT_COMPLETE);
  CHECK(!i2c_controller_start_write(
    &controller,
    UINT8_C(0x48),
    data,
    1u,
    106u
  ));
  CHECK(i2c_controller_take_result(&controller) ==
    I2C_CONTROLLER_RESULT_COMPLETE);
  CHECK(controller.result == I2C_CONTROLLER_RESULT_NONE);
  CHECK(i2c_controller_start_write(
    &controller,
    UINT8_C(0x48),
    data,
    1u,
    106u
  ));
  CHECK(mock_i2c_data_write_count() == 4u);
  CHECK(mock_i2c_data_at(0u) == UINT8_C(0x90));
  CHECK(mock_i2c_data_at(1u) == UINT8_C(0xA1));
  CHECK(mock_i2c_data_at(2u) == UINT8_C(0xB2));
  CHECK(mock_i2c_data_at(3u) == UINT8_C(0xC3));
  CHECK(!mock_i2c_invalid_access());
  return true;
}

static bool test_fault_priority_arbitration_and_bus_recovery(void) {
  i2c_controller_t controller = { 0 };
  const uint8_t data[] = { UINT8_C(0x55) };
  size_t offset;
  const expected_event_t arbitration_events[] = {
    {
      MOCK_I2C_EVENT_STATUS_READ,
      I2C0_STATUS_DATA_ACK | I2C0_STATUS_NACK | I2C0_STATUS_ARBITRATION_LOST |
        I2C0_STATUS_BUS_ERROR,
    },
    { MOCK_I2C_EVENT_STATUS_CLEAR, I2C0_STATUS_ALL },
    { MOCK_I2C_EVENT_CONTROL_WRITE, I2C0_CONTROL_ENABLE },
  };
  const expected_event_t bus_error_events[] = {
    {
      MOCK_I2C_EVENT_STATUS_READ,
      I2C0_STATUS_NACK | I2C0_STATUS_BUS_ERROR,
    },
    { MOCK_I2C_EVENT_STATUS_CLEAR, I2C0_STATUS_ALL },
    {
      MOCK_I2C_EVENT_CONTROL_WRITE,
      I2C0_CONTROL_ENABLE | I2C0_CONTROL_STOP,
    },
  };
  const expected_event_t nack_events[] = {
    { MOCK_I2C_EVENT_STATUS_READ, I2C0_STATUS_NACK },
    { MOCK_I2C_EVENT_STATUS_CLEAR, I2C0_STATUS_ALL },
    {
      MOCK_I2C_EVENT_CONTROL_WRITE,
      I2C0_CONTROL_ENABLE | I2C0_CONTROL_STOP,
    },
  };

  mock_i2c_reset();
  CHECK(initialize(&controller));
  CHECK(i2c_controller_start_write(
    &controller,
    UINT8_C(0x48),
    data,
    sizeof(data),
    0u
  ));
  offset = mock_i2c_event_count();
  mock_i2c_set_status(
    I2C0_STATUS_DATA_ACK | I2C0_STATUS_NACK |
      I2C0_STATUS_ARBITRATION_LOST | I2C0_STATUS_BUS_ERROR
  );
  CHECK(i2c_controller_poll(&controller, 1u) ==
    I2C_CONTROLLER_RESULT_ARBITRATION_LOST);
  CHECK(events_match_from(
    offset,
    arbitration_events,
    sizeof(arbitration_events) / sizeof(arbitration_events[0])
  ));
  CHECK(mock_i2c_status() == 0u);
  CHECK(mock_i2c_control() == I2C0_CONTROL_ENABLE);
  CHECK(transaction_is_clear(&controller));
  CHECK(!i2c_controller_start_write(
    &controller,
    UINT8_C(0x48),
    data,
    sizeof(data),
    2u
  ));
  CHECK(i2c_controller_take_result(&controller) ==
    I2C_CONTROLLER_RESULT_ARBITRATION_LOST);

  CHECK(i2c_controller_start_write(
    &controller,
    UINT8_C(0x48),
    data,
    sizeof(data),
    3u
  ));
  offset = mock_i2c_event_count();
  mock_i2c_set_status(I2C0_STATUS_NACK | I2C0_STATUS_BUS_ERROR);
  CHECK(i2c_controller_poll(&controller, 4u) ==
    I2C_CONTROLLER_RESULT_BUS_ERROR);
  CHECK(events_match_from(
    offset,
    bus_error_events,
    sizeof(bus_error_events) / sizeof(bus_error_events[0])
  ));
  CHECK(i2c_controller_take_result(&controller) ==
    I2C_CONTROLLER_RESULT_BUS_ERROR);

  CHECK(i2c_controller_start_write(
    &controller,
    UINT8_C(0x48),
    data,
    sizeof(data),
    5u
  ));
  offset = mock_i2c_event_count();
  CHECK(mock_i2c_signal_nack());
  CHECK(i2c_controller_poll(&controller, 6u) == I2C_CONTROLLER_RESULT_NACK);
  CHECK(events_match_from(
    offset,
    nack_events,
    sizeof(nack_events) / sizeof(nack_events[0])
  ));
  CHECK(i2c_controller_take_result(&controller) == I2C_CONTROLLER_RESULT_NACK);
  CHECK(!mock_i2c_invalid_access());
  return true;
}

static bool test_wrap_safe_timeout_and_completion_priority(void) {
  i2c_controller_t controller = { 0 };
  const uint8_t data[] = { UINT8_C(0x9A) };
  size_t offset;
  const expected_event_t idle_poll_events[] = {
    { MOCK_I2C_EVENT_STATUS_READ, 0u },
  };
  const expected_event_t timeout_events[] = {
    { MOCK_I2C_EVENT_STATUS_READ, 0u },
    {
      MOCK_I2C_EVENT_CONTROL_WRITE,
      I2C0_CONTROL_ENABLE | I2C0_CONTROL_STOP,
    },
    { MOCK_I2C_EVENT_STATUS_CLEAR, I2C0_STATUS_ALL },
  };

  mock_i2c_reset();
  CHECK(i2c_controller_poll(NULL, 0u) == I2C_CONTROLLER_RESULT_NONE);
  CHECK(i2c_controller_take_result(NULL) == I2C_CONTROLLER_RESULT_NONE);
  CHECK(initialize(&controller));
  offset = mock_i2c_event_count();
  CHECK(i2c_controller_poll(&controller, 0u) == I2C_CONTROLLER_RESULT_NONE);
  CHECK(mock_i2c_event_count() == offset);

  CHECK(i2c_controller_start_write(
    &controller,
    UINT8_C(0x48),
    data,
    sizeof(data),
    UINT32_MAX - UINT32_C(10)
  ));
  offset = mock_i2c_event_count();
  CHECK(i2c_controller_poll(&controller, UINT32_C(13)) ==
    I2C_CONTROLLER_RESULT_NONE);
  CHECK(events_match_from(
    offset,
    idle_poll_events,
    sizeof(idle_poll_events) / sizeof(idle_poll_events[0])
  ));
  CHECK(controller.busy);

  offset = mock_i2c_event_count();
  CHECK(i2c_controller_poll(&controller, UINT32_C(14)) ==
    I2C_CONTROLLER_RESULT_TIMED_OUT);
  CHECK(events_match_from(
    offset,
    timeout_events,
    sizeof(timeout_events) / sizeof(timeout_events[0])
  ));
  CHECK(transaction_is_clear(&controller));
  CHECK(!i2c_controller_start_write(
    &controller,
    UINT8_C(0x48),
    data,
    sizeof(data),
    20u
  ));
  CHECK(i2c_controller_take_result(&controller) ==
    I2C_CONTROLLER_RESULT_TIMED_OUT);

  CHECK(i2c_controller_start_write(
    &controller,
    UINT8_C(0x48),
    data,
    sizeof(data),
    200u
  ));
  CHECK(mock_i2c_signal_start());
  CHECK(i2c_controller_poll(&controller, 201u) == I2C_CONTROLLER_RESULT_NONE);
  CHECK(mock_i2c_signal_address_ack());
  CHECK(i2c_controller_poll(&controller, 202u) == I2C_CONTROLLER_RESULT_NONE);
  CHECK(mock_i2c_signal_data_ack());
  CHECK(i2c_controller_poll(
    &controller,
    200u + I2C_CONTROLLER_TIMEOUT_MS
  ) == I2C_CONTROLLER_RESULT_COMPLETE);
  CHECK(i2c_controller_take_result(&controller) ==
    I2C_CONTROLLER_RESULT_COMPLETE);
  CHECK(!mock_i2c_invalid_access());
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "initialization validation and recovery", test_initialization_validation_and_recovery },
    { "start validation, stale status, and busy rejection", test_start_validation_stale_status_and_busy_rejection },
    { "write protocol completion and result consumption", test_write_protocol_completion_and_result_consumption },
    { "fault priority, arbitration, and bus recovery", test_fault_priority_arbitration_and_bus_recovery },
    { "wrap-safe timeout and completion priority", test_wrap_safe_timeout_and_completion_priority },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) return 1;
    printf("ok - %s\n", tests[index].name);
  }
  return 0;
}

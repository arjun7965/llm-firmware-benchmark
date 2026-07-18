#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "can_controller.h"
#include "mock_can_controller.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
              __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

typedef struct {
  mock_can_event_t event;
  uint32_t value;
} expected_event_t;

static bool initialize(can_controller_t *controller) {
  return can_controller_init(controller, mock_can0());
}

static bool events_match_from(
  size_t offset,
  const expected_event_t *expected,
  size_t expected_count
) {
  if (mock_can_event_count() != offset + expected_count) return false;
  for (size_t index = 0u; index < expected_count; index++) {
    if (mock_can_event_at(offset + index) != expected[index].event ||
      mock_can_event_value(offset + index) != expected[index].value) {
      return false;
    }
  }
  return true;
}

static bool frames_equal(const can_frame_t *left, const can_frame_t *right) {
  if (left->identifier != right->identifier || left->dlc != right->dlc) {
    return false;
  }
  for (uint8_t index = 0u; index < CAN0_MAX_DLC; index++) {
    if (left->data[index] != right->data[index]) return false;
  }
  return true;
}

static bool test_initialization_validation_and_reinitialization(void) {
  can_controller_t controller = {
    .can = (volatile can0_registers_t *)(uintptr_t)UINT32_C(1),
    .received = {
      .identifier = UINT16_C(0x123),
      .dlc = 2u,
      .data = { UINT8_C(0xAA), UINT8_C(0x55) },
    },
    .error_flags = UINT32_MAX,
    .rx_dropped = 9u,
    .state = CAN_CONTROLLER_STATE_BUS_OFF,
    .result = CAN_CONTROLLER_RESULT_BUS_OFF,
    .rx_pending = true,
    .initialized = true,
  };
  const can_frame_t frame = {
    .identifier = UINT16_C(0x245),
    .dlc = 1u,
    .data = { UINT8_C(0x99) },
  };
  const expected_event_t initialization_events[] = {
    { MOCK_CAN_EVENT_CONTROL_WRITE, 0u },
    { MOCK_CAN_EVENT_STATUS_CLEAR, CAN0_STATUS_EVENT_MASK },
    { MOCK_CAN_EVENT_BIT_TIMING_WRITE, CAN0_BIT_TIMING_500K },
    { MOCK_CAN_EVENT_ACCEPTANCE_FILTER_WRITE, CAN0_ACCEPT_ALL_STANDARD },
    { MOCK_CAN_EVENT_CONTROL_WRITE, CAN0_CONTROL_ACTIVE },
  };

  mock_can_reset();
  CHECK(!can_controller_init(NULL, mock_can0()));
  CHECK(!can_controller_init(&controller, NULL));
  CHECK(controller.can == (volatile can0_registers_t *)(uintptr_t)UINT32_C(1));
  CHECK(controller.state == CAN_CONTROLLER_STATE_BUS_OFF);
  CHECK(controller.result == CAN_CONTROLLER_RESULT_BUS_OFF);
  CHECK(mock_can_event_count() == 0u);
  CHECK(mock_can_irq_save_count() == 0u);

  CHECK(initialize(&controller));
  CHECK(controller.can == mock_can0());
  CHECK(controller.received.identifier == 0u);
  CHECK(controller.received.dlc == 0u);
  CHECK(controller.error_flags == 0u);
  CHECK(controller.rx_dropped == 0u);
  CHECK(controller.state == CAN_CONTROLLER_STATE_READY);
  CHECK(controller.result == CAN_CONTROLLER_RESULT_NONE);
  CHECK(!controller.rx_pending);
  CHECK(controller.initialized);
  CHECK(events_match_from(
    0u,
    initialization_events,
    sizeof(initialization_events) / sizeof(initialization_events[0])
  ));
  CHECK(mock_can_control_write_count() == 2u);
  CHECK(mock_can_control_at(0u) == 0u);
  CHECK(mock_can_control_at(1u) == CAN0_CONTROL_ACTIVE);
  CHECK(mock_can_bit_timing_write_count() == 1u);
  CHECK(mock_can_bit_timing() == CAN0_BIT_TIMING_500K);
  CHECK(mock_can_acceptance_filter_write_count() == 1u);
  CHECK(mock_can_acceptance_filter() == CAN0_ACCEPT_ALL_STANDARD);
  CHECK(mock_can_status_clear_write_count() == 1u);
  CHECK(mock_can_last_status_clear() == CAN0_STATUS_EVENT_MASK);
  CHECK(mock_can_irq_save_count() == 0u);

  CHECK(can_controller_send(&controller, &frame));
  CHECK(controller.state == CAN_CONTROLLER_STATE_TX_PENDING);
  CHECK(initialize(&controller));
  CHECK(controller.state == CAN_CONTROLLER_STATE_READY);
  CHECK(controller.result == CAN_CONTROLLER_RESULT_NONE);
  CHECK(!controller.rx_pending);
  CHECK(mock_can_control() == CAN0_CONTROL_ACTIVE);
  CHECK(!mock_can_invalid_access());
  return true;
}

static bool test_send_validation_ordering_and_busy_rejection(void) {
  can_controller_t controller = { 0 };
  can_controller_t uninitialized = { 0 };
  const can_frame_t valid = {
    .identifier = UINT16_C(0x321),
    .dlc = 3u,
    .data = { UINT8_C(0x11), UINT8_C(0x22), UINT8_C(0x33) },
  };
  can_frame_t invalid_identifier = valid;
  can_frame_t invalid_dlc = valid;
  size_t saves;
  size_t restores;
  size_t offset;
  const expected_event_t send_events[] = {
    {
      MOCK_CAN_EVENT_STATUS_CLEAR,
      CAN0_STATUS_TX_COMPLETE | CAN0_STATUS_TX_ERROR,
    },
    { MOCK_CAN_EVENT_TX_IDENTIFIER_WRITE, UINT16_C(0x321) },
    { MOCK_CAN_EVENT_TX_DLC_WRITE, 3u },
    { MOCK_CAN_EVENT_TX_DATA_WRITE, UINT8_C(0x11) },
    { MOCK_CAN_EVENT_TX_DATA_WRITE, UINT8_C(0x22) },
    { MOCK_CAN_EVENT_TX_DATA_WRITE, UINT8_C(0x33) },
    {
      MOCK_CAN_EVENT_CONTROL_WRITE,
      CAN0_CONTROL_ACTIVE | CAN0_CONTROL_TX_IRQ_ENABLE,
    },
  };

  invalid_identifier.identifier = (uint16_t)(CAN0_STANDARD_ID_MAX + 1u);
  invalid_dlc.dlc = CAN0_MAX_DLC + 1u;
  mock_can_reset();
  CHECK(!can_controller_send(&uninitialized, &valid));
  CHECK(mock_can_irq_save_count() == 0u);
  CHECK(initialize(&controller));
  saves = mock_can_irq_save_count();
  restores = mock_can_irq_restore_count();
  offset = mock_can_event_count();
  CHECK(!can_controller_send(&controller, NULL));
  CHECK(!can_controller_send(&controller, &invalid_identifier));
  CHECK(!can_controller_send(&controller, &invalid_dlc));
  CHECK(mock_can_irq_save_count() == saves);
  CHECK(mock_can_irq_restore_count() == restores);
  CHECK(mock_can_event_count() == offset);

  mock_can_set_status(CAN0_STATUS_TX_COMPLETE | CAN0_STATUS_TX_ERROR);
  mock_can_set_interrupts_enabled(false);
  offset = mock_can_event_count();
  CHECK(can_controller_send(&controller, &valid));
  CHECK(events_match_from(
    offset,
    send_events,
    sizeof(send_events) / sizeof(send_events[0])
  ));
  CHECK(controller.state == CAN_CONTROLLER_STATE_TX_PENDING);
  CHECK(controller.result == CAN_CONTROLLER_RESULT_NONE);
  CHECK(mock_can_status() == 0u);
  CHECK(mock_can_tx_identifier() == valid.identifier);
  CHECK(mock_can_tx_dlc() == valid.dlc);
  CHECK(mock_can_tx_data_write_count() == valid.dlc);
  CHECK(mock_can_tx_data_at(0u) == valid.data[0]);
  CHECK(mock_can_tx_data_at(1u) == valid.data[1]);
  CHECK(mock_can_tx_data_at(2u) == valid.data[2]);
  CHECK(mock_can_irq_save_count() == saves + 1u);
  CHECK(mock_can_irq_restore_count() == restores + 1u);
  CHECK(mock_can_last_irq_restore() == 0u);
  CHECK(!mock_can_interrupts_enabled());

  saves = mock_can_irq_save_count();
  restores = mock_can_irq_restore_count();
  offset = mock_can_event_count();
  CHECK(!can_controller_send(&controller, &valid));
  CHECK(mock_can_irq_save_count() == saves + 1u);
  CHECK(mock_can_irq_restore_count() == restores + 1u);
  CHECK(mock_can_event_count() == offset);
  CHECK(!mock_can_interrupts_enabled());
  CHECK(!mock_can_invalid_access());
  return true;
}

static bool test_tx_results_priority_and_consumption(void) {
  can_controller_t controller = { 0 };
  const can_frame_t frame = {
    .identifier = UINT16_C(0x456),
    .dlc = 2u,
    .data = { UINT8_C(0xA5), UINT8_C(0x5A) },
  };
  uint32_t error_flags = UINT32_MAX;
  size_t saves;
  size_t restores;
  size_t offset;
  const expected_event_t complete_events[] = {
    { MOCK_CAN_EVENT_STATUS_READ, CAN0_STATUS_TX_COMPLETE },
    { MOCK_CAN_EVENT_STATUS_CLEAR, CAN0_STATUS_TX_COMPLETE },
    { MOCK_CAN_EVENT_CONTROL_WRITE, CAN0_CONTROL_ACTIVE },
  };
  const expected_event_t error_events[] = {
    {
      MOCK_CAN_EVENT_STATUS_READ,
      CAN0_STATUS_TX_COMPLETE | CAN0_STATUS_TX_ERROR,
    },
    {
      MOCK_CAN_EVENT_STATUS_CLEAR,
      CAN0_STATUS_TX_COMPLETE | CAN0_STATUS_TX_ERROR,
    },
    { MOCK_CAN_EVENT_CONTROL_WRITE, CAN0_CONTROL_ACTIVE },
  };

  mock_can_reset();
  CHECK(initialize(&controller));
  CHECK(can_controller_send(&controller, &frame));
  saves = mock_can_irq_save_count();
  restores = mock_can_irq_restore_count();
  CHECK(mock_can_signal_tx_complete());
  offset = mock_can_event_count();
  can_controller_irq(&controller);
  CHECK(events_match_from(
    offset,
    complete_events,
    sizeof(complete_events) / sizeof(complete_events[0])
  ));
  CHECK(controller.state == CAN_CONTROLLER_STATE_READY);
  CHECK(controller.result == CAN_CONTROLLER_RESULT_TX_COMPLETE);
  CHECK(controller.error_flags == 0u);
  CHECK(mock_can_irq_save_count() == saves);
  CHECK(mock_can_irq_restore_count() == restores);

  offset = mock_can_event_count();
  CHECK(!can_controller_send(&controller, &frame));
  CHECK(mock_can_event_count() == offset);
  mock_can_set_interrupts_enabled(false);
  CHECK(can_controller_take_result(&controller, &error_flags) ==
    CAN_CONTROLLER_RESULT_TX_COMPLETE);
  CHECK(error_flags == 0u);
  CHECK(controller.result == CAN_CONTROLLER_RESULT_NONE);
  CHECK(!mock_can_interrupts_enabled());
  CHECK(mock_can_last_irq_restore() == 0u);

  CHECK(can_controller_send(&controller, &frame));
  mock_can_set_status(CAN0_STATUS_TX_COMPLETE | CAN0_STATUS_TX_ERROR);
  offset = mock_can_event_count();
  can_controller_irq(&controller);
  CHECK(events_match_from(
    offset,
    error_events,
    sizeof(error_events) / sizeof(error_events[0])
  ));
  CHECK(controller.state == CAN_CONTROLLER_STATE_READY);
  CHECK(controller.result == CAN_CONTROLLER_RESULT_TX_ERROR);
  CHECK(controller.error_flags == CAN0_STATUS_TX_ERROR);
  CHECK(can_controller_take_result(&controller, &error_flags) ==
    CAN_CONTROLLER_RESULT_TX_ERROR);
  CHECK(error_flags == CAN0_STATUS_TX_ERROR);
  CHECK(controller.error_flags == 0u);
  CHECK(controller.result == CAN_CONTROLLER_RESULT_NONE);
  CHECK(!mock_can_invalid_access());
  return true;
}

static bool test_receive_delivery_drop_and_interleaved_tx(void) {
  can_controller_t controller = { 0 };
  const uint8_t first_data[] = { UINT8_C(0x01), UINT8_C(0x02) };
  const uint8_t second_data[] = { UINT8_C(0x10) };
  const uint8_t third_data[] = { UINT8_C(0x77) };
  const can_frame_t first_frame = {
    .identifier = UINT16_C(0x201),
    .dlc = 2u,
    .data = { UINT8_C(0x01), UINT8_C(0x02) },
  };
  const can_frame_t third_frame = {
    .identifier = UINT16_C(0x321),
    .dlc = 1u,
    .data = { UINT8_C(0x77) },
  };
  const can_frame_t tx_frame = {
    .identifier = UINT16_C(0x456),
    .dlc = 0u,
  };
  can_frame_t received = {
    .identifier = UINT16_MAX,
    .dlc = UINT8_MAX,
    .data = { UINT8_MAX, UINT8_MAX, UINT8_MAX, UINT8_MAX,
      UINT8_MAX, UINT8_MAX, UINT8_MAX, UINT8_MAX },
  };
  uint32_t error_flags = UINT32_MAX;
  size_t offset;
  const expected_event_t receive_events[] = {
    { MOCK_CAN_EVENT_STATUS_READ, CAN0_STATUS_RX_PENDING },
    { MOCK_CAN_EVENT_STATUS_CLEAR, CAN0_STATUS_RX_PENDING },
    { MOCK_CAN_EVENT_RX_IDENTIFIER_READ, UINT16_C(0x201) },
    { MOCK_CAN_EVENT_RX_DLC_READ, 2u },
    { MOCK_CAN_EVENT_RX_DATA_READ, UINT8_C(0x01) },
    { MOCK_CAN_EVENT_RX_DATA_READ, UINT8_C(0x02) },
  };
  const expected_event_t drop_events[] = {
    { MOCK_CAN_EVENT_STATUS_READ, CAN0_STATUS_RX_PENDING },
    { MOCK_CAN_EVENT_STATUS_CLEAR, CAN0_STATUS_RX_PENDING },
    { MOCK_CAN_EVENT_RX_IDENTIFIER_READ, UINT16_C(0x202) },
    { MOCK_CAN_EVENT_RX_DLC_READ, 1u },
    { MOCK_CAN_EVENT_RX_DATA_READ, UINT8_C(0x10) },
  };
  const expected_event_t interleaved_events[] = {
    {
      MOCK_CAN_EVENT_STATUS_READ,
      CAN0_STATUS_RX_PENDING | CAN0_STATUS_TX_COMPLETE,
    },
    {
      MOCK_CAN_EVENT_STATUS_CLEAR,
      CAN0_STATUS_RX_PENDING | CAN0_STATUS_TX_COMPLETE,
    },
    { MOCK_CAN_EVENT_RX_IDENTIFIER_READ, UINT16_C(0x321) },
    { MOCK_CAN_EVENT_RX_DLC_READ, 1u },
    { MOCK_CAN_EVENT_RX_DATA_READ, UINT8_C(0x77) },
    { MOCK_CAN_EVENT_CONTROL_WRITE, CAN0_CONTROL_ACTIVE },
  };

  mock_can_reset();
  CHECK(initialize(&controller));
  CHECK(mock_can_inject_rx(first_frame.identifier, first_frame.dlc, first_data));
  offset = mock_can_event_count();
  can_controller_irq(&controller);
  CHECK(events_match_from(
    offset,
    receive_events,
    sizeof(receive_events) / sizeof(receive_events[0])
  ));
  CHECK(controller.rx_pending);
  mock_can_set_interrupts_enabled(false);
  CHECK(can_controller_take_received(&controller, &received));
  CHECK(frames_equal(&received, &first_frame));
  CHECK(!controller.rx_pending);
  CHECK(!mock_can_interrupts_enabled());

  CHECK(mock_can_inject_rx(first_frame.identifier, first_frame.dlc, first_data));
  can_controller_irq(&controller);
  CHECK(controller.rx_pending);
  CHECK(mock_can_inject_rx(UINT16_C(0x202), 1u, second_data));
  offset = mock_can_event_count();
  can_controller_irq(&controller);
  CHECK(events_match_from(
    offset,
    drop_events,
    sizeof(drop_events) / sizeof(drop_events[0])
  ));
  CHECK(can_controller_rx_dropped(&controller) == 1u);
  CHECK(can_controller_take_received(&controller, &received));

  CHECK(can_controller_send(&controller, &tx_frame));
  CHECK(mock_can_inject_rx(third_frame.identifier, third_frame.dlc, third_data));
  CHECK(mock_can_signal_tx_complete());
  offset = mock_can_event_count();
  can_controller_irq(&controller);
  CHECK(events_match_from(
    offset,
    interleaved_events,
    sizeof(interleaved_events) / sizeof(interleaved_events[0])
  ));
  CHECK(controller.state == CAN_CONTROLLER_STATE_READY);
  CHECK(controller.result == CAN_CONTROLLER_RESULT_TX_COMPLETE);
  CHECK(controller.rx_pending);
  CHECK(can_controller_take_result(&controller, &error_flags) ==
    CAN_CONTROLLER_RESULT_TX_COMPLETE);
  CHECK(error_flags == 0u);
  CHECK(can_controller_take_received(&controller, &received));
  CHECK(frames_equal(&received, &third_frame));
  CHECK(!mock_can_invalid_access());
  return true;
}

static bool test_bus_off_recovery_and_terminal_gate(void) {
  can_controller_t controller = { 0 };
  const can_frame_t frame = {
    .identifier = UINT16_C(0x120),
    .dlc = 1u,
    .data = { UINT8_C(0x42) },
  };
  uint32_t error_flags = 0u;
  size_t offset;
  size_t saves;
  size_t restores;
  const expected_event_t bus_off_events[] = {
    {
      MOCK_CAN_EVENT_STATUS_READ,
      CAN0_STATUS_RX_PENDING | CAN0_STATUS_TX_COMPLETE |
        CAN0_STATUS_TX_ERROR | CAN0_STATUS_BUS_OFF,
    },
    {
      MOCK_CAN_EVENT_STATUS_CLEAR,
      CAN0_STATUS_RX_PENDING | CAN0_STATUS_TX_COMPLETE |
        CAN0_STATUS_TX_ERROR | CAN0_STATUS_BUS_OFF,
    },
    { MOCK_CAN_EVENT_CONTROL_WRITE, 0u },
  };
  const expected_event_t recovery_events[] = {
    { MOCK_CAN_EVENT_STATUS_READ, CAN0_STATUS_RECOVERY_READY },
    { MOCK_CAN_EVENT_CONTROL_WRITE, 0u },
    { MOCK_CAN_EVENT_STATUS_CLEAR, CAN0_STATUS_EVENT_MASK },
    { MOCK_CAN_EVENT_BIT_TIMING_WRITE, CAN0_BIT_TIMING_500K },
    { MOCK_CAN_EVENT_ACCEPTANCE_FILTER_WRITE, CAN0_ACCEPT_ALL_STANDARD },
    { MOCK_CAN_EVENT_CONTROL_WRITE, CAN0_CONTROL_ACTIVE },
  };

  mock_can_reset();
  CHECK(!can_controller_recover(NULL));
  CHECK(initialize(&controller));
  saves = mock_can_irq_save_count();
  offset = mock_can_event_count();
  CHECK(!can_controller_recover(&controller));
  CHECK(mock_can_irq_save_count() == saves + 1u);
  CHECK(mock_can_event_count() == offset);

  CHECK(can_controller_send(&controller, &frame));
  mock_can_set_status(
    CAN0_STATUS_RX_PENDING | CAN0_STATUS_TX_COMPLETE |
      CAN0_STATUS_TX_ERROR | CAN0_STATUS_BUS_OFF
  );
  saves = mock_can_irq_save_count();
  restores = mock_can_irq_restore_count();
  offset = mock_can_event_count();
  can_controller_irq(&controller);
  CHECK(events_match_from(
    offset,
    bus_off_events,
    sizeof(bus_off_events) / sizeof(bus_off_events[0])
  ));
  CHECK(controller.state == CAN_CONTROLLER_STATE_BUS_OFF);
  CHECK(controller.result == CAN_CONTROLLER_RESULT_BUS_OFF);
  CHECK(controller.error_flags == CAN0_STATUS_BUS_OFF);
  CHECK(mock_can_control() == 0u);
  CHECK(mock_can_status() == 0u);
  CHECK(mock_can_irq_save_count() == saves);
  CHECK(mock_can_irq_restore_count() == restores);

  offset = mock_can_event_count();
  CHECK(!can_controller_send(&controller, &frame));
  CHECK(mock_can_event_count() == offset);
  mock_can_set_status(CAN0_STATUS_BUS_OFF | CAN0_STATUS_RECOVERY_READY);
  offset = mock_can_event_count();
  CHECK(!can_controller_recover(&controller));
  CHECK(mock_can_event_count() == offset + 1u);
  CHECK(mock_can_event_at(offset) == MOCK_CAN_EVENT_STATUS_READ);

  mock_can_set_status(CAN0_STATUS_RECOVERY_READY);
  mock_can_set_interrupts_enabled(false);
  offset = mock_can_event_count();
  CHECK(can_controller_recover(&controller));
  CHECK(events_match_from(
    offset,
    recovery_events,
    sizeof(recovery_events) / sizeof(recovery_events[0])
  ));
  CHECK(controller.state == CAN_CONTROLLER_STATE_READY);
  CHECK(controller.result == CAN_CONTROLLER_RESULT_BUS_OFF);
  CHECK(!mock_can_interrupts_enabled());
  CHECK(mock_can_last_irq_restore() == 0u);

  CHECK(!can_controller_send(&controller, &frame));
  CHECK(can_controller_take_result(&controller, &error_flags) ==
    CAN_CONTROLLER_RESULT_BUS_OFF);
  CHECK(error_flags == CAN0_STATUS_BUS_OFF);
  CHECK(controller.result == CAN_CONTROLLER_RESULT_NONE);
  CHECK(can_controller_send(&controller, &frame));
  CHECK(!mock_can_invalid_access());
  return true;
}

static bool test_idle_irq_and_invalid_foreground_calls(void) {
  can_controller_t controller = { 0 };
  can_controller_t uninitialized = { 0 };
  can_frame_t frame = {
    .identifier = UINT16_C(0x111),
    .dlc = 1u,
    .data = { UINT8_C(0x88) },
  };
  uint32_t error_flags = UINT32_MAX;
  size_t saves;
  size_t restores;
  size_t reads;
  size_t clears;

  mock_can_reset();
  can_controller_irq(NULL);
  can_controller_irq(&uninitialized);
  CHECK(can_controller_take_result(NULL, &error_flags) ==
    CAN_CONTROLLER_RESULT_NONE);
  CHECK(can_controller_take_result(&uninitialized, &error_flags) ==
    CAN_CONTROLLER_RESULT_NONE);
  CHECK(!can_controller_take_received(NULL, &frame));
  CHECK(!can_controller_take_received(&uninitialized, &frame));
  CHECK(can_controller_rx_dropped(NULL) == 0u);
  CHECK(mock_can_irq_save_count() == 0u);
  CHECK(mock_can_status_read_count() == 0u);

  CHECK(initialize(&controller));
  saves = mock_can_irq_save_count();
  restores = mock_can_irq_restore_count();
  CHECK(can_controller_take_result(&controller, NULL) ==
    CAN_CONTROLLER_RESULT_NONE);
  CHECK(!can_controller_take_received(&controller, NULL));
  CHECK(mock_can_irq_save_count() == saves);
  CHECK(mock_can_irq_restore_count() == restores);

  reads = mock_can_status_read_count();
  clears = mock_can_status_clear_write_count();
  can_controller_irq(&controller);
  CHECK(mock_can_status_read_count() == reads + 1u);
  CHECK(mock_can_status_clear_write_count() == clears);
  mock_can_set_interrupts_enabled(false);
  CHECK(!can_controller_take_received(&controller, &frame));
  CHECK(!mock_can_interrupts_enabled());
  CHECK(can_controller_rx_dropped(&controller) == 0u);
  CHECK(!mock_can_interrupts_enabled());
  CHECK(!mock_can_invalid_access());
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "initialization validation and reinitialization", test_initialization_validation_and_reinitialization },
    { "send validation, ordering, and busy rejection", test_send_validation_ordering_and_busy_rejection },
    { "TX result priority and consumption", test_tx_results_priority_and_consumption },
    { "receive delivery, drop, and interleaved TX", test_receive_delivery_drop_and_interleaved_tx },
    { "bus-off recovery and terminal gate", test_bus_off_recovery_and_terminal_gate },
    { "idle IRQ and invalid foreground calls", test_idle_irq_and_invalid_foreground_calls },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) return 1;
    printf("ok - %s\n", tests[index].name);
  }
  return 0;
}

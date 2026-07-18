#include "can_controller.h"

static bool controller_is_ready(const can_controller_t *controller) {
  return controller != NULL && controller->initialized &&
    controller->can != NULL;
}

static bool frame_is_valid(const can_frame_t *frame) {
  return frame != NULL && frame->identifier <= CAN0_STANDARD_ID_MAX &&
    frame->dlc <= CAN0_MAX_DLC;
}

static void configure_controller(can_controller_t *controller) {
  can0_write_control(controller->can, 0u);
  can0_write_status_clear(controller->can, CAN0_STATUS_EVENT_MASK);
  can0_write_bit_timing(controller->can, CAN0_BIT_TIMING_500K);
  can0_write_acceptance_filter(controller->can, CAN0_ACCEPT_ALL_STANDARD);
  can0_write_control(controller->can, CAN0_CONTROL_ACTIVE);
}

static void finish_transmit(
  can_controller_t *controller,
  can_controller_result_t result
) {
  controller->state = CAN_CONTROLLER_STATE_READY;
  can0_write_control(controller->can, CAN0_CONTROL_ACTIVE);
  controller->result = result;
}

static void enter_bus_off(can_controller_t *controller) {
  controller->state = CAN_CONTROLLER_STATE_BUS_OFF;
  controller->error_flags |= CAN0_STATUS_BUS_OFF;
  controller->result = CAN_CONTROLLER_RESULT_BUS_OFF;
  can0_write_control(controller->can, 0u);
}

static void receive_frame(can_controller_t *controller) {
  if (controller->rx_pending) {
    (void)can0_read_rx_identifier(controller->can);
    const uint8_t dropped_dlc = can0_read_rx_dlc(controller->can);
    for (uint8_t index = 0u; index < dropped_dlc; index++) {
      (void)can0_read_rx_data(controller->can);
    }
    controller->rx_dropped++;
    return;
  }

  can_frame_t frame = {
    .identifier = can0_read_rx_identifier(controller->can),
    .dlc = can0_read_rx_dlc(controller->can),
  };
  for (uint8_t index = 0u; index < frame.dlc; index++) {
    frame.data[index] = can0_read_rx_data(controller->can);
  }
  controller->received = frame;
  controller->rx_pending = true;
}

bool can_controller_init(
  can_controller_t *controller,
  volatile can0_registers_t *can
) {
  if (controller == NULL || can == NULL) return false;

  *controller = (can_controller_t) {
    .can = can,
  };
  configure_controller(controller);
  controller->initialized = true;
  return true;
}

bool can_controller_send(
  can_controller_t *controller,
  const can_frame_t *frame
) {
  if (!controller_is_ready(controller) || !frame_is_valid(frame)) return false;

  const uint32_t previous_interrupt_state = can0_irq_save_disable();
  bool sent = false;
  if (
    controller->state == CAN_CONTROLLER_STATE_READY &&
    controller->result == CAN_CONTROLLER_RESULT_NONE
  ) {
    controller->error_flags = 0u;
    can0_write_status_clear(
      controller->can,
      CAN0_STATUS_TX_COMPLETE | CAN0_STATUS_TX_ERROR
    );
    can0_write_tx_identifier(controller->can, frame->identifier);
    can0_write_tx_dlc(controller->can, frame->dlc);
    for (uint8_t index = 0u; index < frame->dlc; index++) {
      can0_write_tx_data(controller->can, frame->data[index]);
    }
    can0_write_control(
      controller->can,
      CAN0_CONTROL_ACTIVE | CAN0_CONTROL_TX_IRQ_ENABLE
    );
    controller->state = CAN_CONTROLLER_STATE_TX_PENDING;
    sent = true;
  }
  can0_irq_restore(previous_interrupt_state);
  return sent;
}

void can_controller_irq(can_controller_t *controller) {
  if (!controller_is_ready(controller)) return;

  const uint32_t observed = can0_read_status(controller->can) &
    CAN0_STATUS_EVENT_MASK;
  if (observed == 0u) return;

  can0_write_status_clear(controller->can, observed);
  if ((observed & CAN0_STATUS_BUS_OFF) != 0u) {
    enter_bus_off(controller);
    return;
  }
  if ((observed & CAN0_STATUS_RX_PENDING) != 0u) {
    receive_frame(controller);
  }
  if (controller->state != CAN_CONTROLLER_STATE_TX_PENDING) return;

  if ((observed & CAN0_STATUS_TX_ERROR) != 0u) {
    controller->error_flags |= CAN0_STATUS_TX_ERROR;
    finish_transmit(controller, CAN_CONTROLLER_RESULT_TX_ERROR);
  } else if ((observed & CAN0_STATUS_TX_COMPLETE) != 0u) {
    finish_transmit(controller, CAN_CONTROLLER_RESULT_TX_COMPLETE);
  }
}

bool can_controller_take_received(
  can_controller_t *controller,
  can_frame_t *frame
) {
  if (!controller_is_ready(controller) || frame == NULL) return false;

  const uint32_t previous_interrupt_state = can0_irq_save_disable();
  const bool received = controller->rx_pending;
  if (received) {
    *frame = controller->received;
    controller->received = (can_frame_t){ 0 };
    controller->rx_pending = false;
  }
  can0_irq_restore(previous_interrupt_state);
  return received;
}

can_controller_result_t can_controller_take_result(
  can_controller_t *controller,
  uint32_t *error_flags
) {
  if (!controller_is_ready(controller) || error_flags == NULL) {
    return CAN_CONTROLLER_RESULT_NONE;
  }

  const uint32_t previous_interrupt_state = can0_irq_save_disable();
  const can_controller_result_t result = controller->result;
  if (result == CAN_CONTROLLER_RESULT_NONE) {
    *error_flags = 0u;
  } else {
    *error_flags = controller->error_flags;
    controller->error_flags = 0u;
    controller->result = CAN_CONTROLLER_RESULT_NONE;
  }
  can0_irq_restore(previous_interrupt_state);
  return result;
}

bool can_controller_recover(can_controller_t *controller) {
  if (!controller_is_ready(controller)) return false;

  const uint32_t previous_interrupt_state = can0_irq_save_disable();
  bool recovered = false;
  if (controller->state == CAN_CONTROLLER_STATE_BUS_OFF) {
    const uint32_t status = can0_read_status(controller->can);
    if (
      (status & (CAN0_STATUS_BUS_OFF | CAN0_STATUS_RECOVERY_READY)) ==
        CAN0_STATUS_RECOVERY_READY
    ) {
      configure_controller(controller);
      controller->state = CAN_CONTROLLER_STATE_READY;
      recovered = true;
    }
  }
  can0_irq_restore(previous_interrupt_state);
  return recovered;
}

uint32_t can_controller_rx_dropped(can_controller_t *controller) {
  if (!controller_is_ready(controller)) return 0u;

  const uint32_t previous_interrupt_state = can0_irq_save_disable();
  const uint32_t dropped = controller->rx_dropped;
  can0_irq_restore(previous_interrupt_state);
  return dropped;
}

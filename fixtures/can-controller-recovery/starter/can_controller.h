#ifndef CAN_CONTROLLER_H
#define CAN_CONTROLLER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_can_controller.h"

typedef struct {
  uint16_t identifier;
  uint8_t dlc;
  uint8_t data[CAN0_MAX_DLC];
} can_frame_t;

typedef enum {
  CAN_CONTROLLER_STATE_READY = 0,
  CAN_CONTROLLER_STATE_TX_PENDING,
  CAN_CONTROLLER_STATE_BUS_OFF,
} can_controller_state_t;

typedef enum {
  CAN_CONTROLLER_RESULT_NONE = 0,
  CAN_CONTROLLER_RESULT_TX_COMPLETE,
  CAN_CONTROLLER_RESULT_TX_ERROR,
  CAN_CONTROLLER_RESULT_BUS_OFF,
} can_controller_result_t;

typedef struct {
  volatile can0_registers_t *can;
  can_frame_t received;
  uint32_t error_flags;
  uint32_t rx_dropped;
  can_controller_state_t state;
  can_controller_result_t result;
  bool rx_pending;
  bool initialized;
} can_controller_t;

_Static_assert(CAN0_MAX_DLC > 0u, "CAN frames must have a payload bound");

bool can_controller_init(
  can_controller_t *controller,
  volatile can0_registers_t *can
);
bool can_controller_send(
  can_controller_t *controller,
  const can_frame_t *frame
);
void can_controller_irq(can_controller_t *controller);
bool can_controller_take_received(
  can_controller_t *controller,
  can_frame_t *frame
);
can_controller_result_t can_controller_take_result(
  can_controller_t *controller,
  uint32_t *error_flags
);
bool can_controller_recover(can_controller_t *controller);
uint32_t can_controller_rx_dropped(can_controller_t *controller);

#endif

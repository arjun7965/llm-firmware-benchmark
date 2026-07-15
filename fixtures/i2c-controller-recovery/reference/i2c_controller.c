#include "i2c_controller.h"

static bool controller_is_ready(const i2c_controller_t *controller) {
  return controller != NULL && controller->initialized &&
    controller->i2c != NULL;
}

static void clear_transaction(i2c_controller_t *controller) {
  controller->data = NULL;
  controller->length = 0u;
  controller->next_index = 0u;
  controller->address = 0u;
  controller->started_at_ms = 0u;
  controller->phase = I2C_CONTROLLER_PHASE_IDLE;
  controller->busy = false;
}

static void publish_result(
  i2c_controller_t *controller,
  i2c_controller_result_t result
) {
  clear_transaction(controller);
  controller->result = result;
}

static void finish_with_stop(
  i2c_controller_t *controller,
  i2c_controller_result_t result
) {
  i2c0_write_status_clear(controller->i2c, I2C0_STATUS_ALL);
  i2c0_write_control(
    controller->i2c,
    I2C0_CONTROL_ENABLE | I2C0_CONTROL_STOP
  );
  publish_result(controller, result);
}

static void finish_arbitration_lost(i2c_controller_t *controller) {
  i2c0_write_status_clear(controller->i2c, I2C0_STATUS_ALL);
  i2c0_write_control(controller->i2c, I2C0_CONTROL_ENABLE);
  publish_result(controller, I2C_CONTROLLER_RESULT_ARBITRATION_LOST);
}

static void finish_timeout(i2c_controller_t *controller) {
  i2c0_write_control(
    controller->i2c,
    I2C0_CONTROL_ENABLE | I2C0_CONTROL_STOP
  );
  i2c0_write_status_clear(controller->i2c, I2C0_STATUS_ALL);
  publish_result(controller, I2C_CONTROLLER_RESULT_TIMED_OUT);
}

bool i2c_controller_init(
  i2c_controller_t *controller,
  volatile i2c0_registers_t *i2c
) {
  if (controller == NULL || i2c == NULL) return false;

  *controller = (i2c_controller_t) {
    .i2c = i2c,
  };
  i2c0_write_control(i2c, 0u);
  i2c0_write_status_clear(i2c, I2C0_STATUS_ALL);
  i2c0_write_control(i2c, I2C0_CONTROL_ENABLE);
  controller->initialized = true;
  return true;
}

bool i2c_controller_start_write(
  i2c_controller_t *controller,
  uint8_t address,
  const uint8_t *data,
  size_t length,
  uint32_t now_ms
) {
  if (
    !controller_is_ready(controller) || controller->busy ||
    controller->result != I2C_CONTROLLER_RESULT_NONE || data == NULL ||
    length == 0u || length > I2C_CONTROLLER_MAX_WRITE_BYTES ||
    address < I2C_CONTROLLER_MIN_ADDRESS ||
    address > I2C_CONTROLLER_MAX_ADDRESS
  ) {
    return false;
  }

  controller->data = data;
  controller->length = length;
  controller->next_index = 0u;
  controller->address = address;
  controller->started_at_ms = now_ms;
  controller->phase = I2C_CONTROLLER_PHASE_WAIT_START;
  controller->busy = true;
  i2c0_write_status_clear(controller->i2c, I2C0_STATUS_ALL);
  i2c0_write_control(
    controller->i2c,
    I2C0_CONTROL_ENABLE | I2C0_CONTROL_START
  );
  return true;
}

i2c_controller_result_t i2c_controller_poll(
  i2c_controller_t *controller,
  uint32_t now_ms
) {
  if (!controller_is_ready(controller) || !controller->busy) {
    return I2C_CONTROLLER_RESULT_NONE;
  }

  const uint32_t status = i2c0_read_status(controller->i2c) & I2C0_STATUS_ALL;
  if ((status & I2C0_STATUS_ARBITRATION_LOST) != 0u) {
    finish_arbitration_lost(controller);
    return controller->result;
  }
  if ((status & I2C0_STATUS_BUS_ERROR) != 0u) {
    finish_with_stop(controller, I2C_CONTROLLER_RESULT_BUS_ERROR);
    return controller->result;
  }
  if ((status & I2C0_STATUS_NACK) != 0u) {
    finish_with_stop(controller, I2C_CONTROLLER_RESULT_NACK);
    return controller->result;
  }

  if (
    controller->phase == I2C_CONTROLLER_PHASE_WAIT_START &&
    (status & I2C0_STATUS_START) != 0u
  ) {
    i2c0_write_status_clear(controller->i2c, I2C0_STATUS_START);
    i2c0_write_data(controller->i2c, (uint8_t)(controller->address << 1u));
    i2c0_write_control(controller->i2c, I2C0_CONTROL_ENABLE);
    controller->phase = I2C_CONTROLLER_PHASE_WAIT_ADDRESS;
    return I2C_CONTROLLER_RESULT_NONE;
  }
  if (
    controller->phase == I2C_CONTROLLER_PHASE_WAIT_ADDRESS &&
    (status & I2C0_STATUS_ADDRESS_ACK) != 0u
  ) {
    i2c0_write_status_clear(controller->i2c, I2C0_STATUS_ADDRESS_ACK);
    i2c0_write_data(controller->i2c, controller->data[0]);
    i2c0_write_control(controller->i2c, I2C0_CONTROL_ENABLE);
    controller->next_index = 1u;
    controller->phase = I2C_CONTROLLER_PHASE_WAIT_DATA;
    return I2C_CONTROLLER_RESULT_NONE;
  }
  if (
    controller->phase == I2C_CONTROLLER_PHASE_WAIT_DATA &&
    (status & I2C0_STATUS_DATA_ACK) != 0u
  ) {
    i2c0_write_status_clear(controller->i2c, I2C0_STATUS_DATA_ACK);
    if (controller->next_index < controller->length) {
      i2c0_write_data(controller->i2c, controller->data[controller->next_index]);
      controller->next_index++;
      i2c0_write_control(controller->i2c, I2C0_CONTROL_ENABLE);
      return I2C_CONTROLLER_RESULT_NONE;
    }
    i2c0_write_control(
      controller->i2c,
      I2C0_CONTROL_ENABLE | I2C0_CONTROL_STOP
    );
    publish_result(controller, I2C_CONTROLLER_RESULT_COMPLETE);
    return controller->result;
  }

  if ((uint32_t)(now_ms - controller->started_at_ms) >=
      I2C_CONTROLLER_TIMEOUT_MS) {
    finish_timeout(controller);
    return controller->result;
  }
  return I2C_CONTROLLER_RESULT_NONE;
}

i2c_controller_result_t i2c_controller_take_result(
  i2c_controller_t *controller
) {
  if (!controller_is_ready(controller)) return I2C_CONTROLLER_RESULT_NONE;

  const i2c_controller_result_t result = controller->result;
  controller->result = I2C_CONTROLLER_RESULT_NONE;
  return result;
}

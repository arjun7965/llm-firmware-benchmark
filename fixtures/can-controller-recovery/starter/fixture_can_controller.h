#ifndef FIXTURE_CAN_CONTROLLER_H
#define FIXTURE_CAN_CONTROLLER_H

#include <stdint.h>

#define CAN0_BASE_ADDRESS UINT32_C(0x40006400)

#define CAN0_CONTROL_ENABLE UINT32_C(1)
#define CAN0_CONTROL_RX_IRQ_ENABLE (UINT32_C(1) << 1)
#define CAN0_CONTROL_TX_IRQ_ENABLE (UINT32_C(1) << 2)
#define CAN0_CONTROL_ACTIVE \
  (CAN0_CONTROL_ENABLE | CAN0_CONTROL_RX_IRQ_ENABLE)

#define CAN0_STATUS_RX_PENDING UINT32_C(1)
#define CAN0_STATUS_TX_COMPLETE (UINT32_C(1) << 1)
#define CAN0_STATUS_TX_ERROR (UINT32_C(1) << 2)
#define CAN0_STATUS_BUS_OFF (UINT32_C(1) << 3)
#define CAN0_STATUS_RECOVERY_READY (UINT32_C(1) << 4)
#define CAN0_STATUS_EVENT_MASK \
  (CAN0_STATUS_RX_PENDING | CAN0_STATUS_TX_COMPLETE | \
    CAN0_STATUS_TX_ERROR | CAN0_STATUS_BUS_OFF)
#define CAN0_STATUS_ALL \
  (CAN0_STATUS_EVENT_MASK | CAN0_STATUS_RECOVERY_READY)

#define CAN0_BIT_TIMING_500K UINT32_C(0x001C0004)
#define CAN0_ACCEPT_ALL_STANDARD UINT32_C(0)
#define CAN0_STANDARD_ID_MAX UINT16_C(0x07FF)
#define CAN0_MAX_DLC UINT8_C(8)

typedef struct can0_registers can0_registers_t;

uint32_t can0_read_status(const volatile can0_registers_t *can);
void can0_write_status_clear(volatile can0_registers_t *can, uint32_t value);
void can0_write_control(volatile can0_registers_t *can, uint32_t value);
void can0_write_bit_timing(volatile can0_registers_t *can, uint32_t value);
void can0_write_acceptance_filter(
  volatile can0_registers_t *can,
  uint32_t value
);
void can0_write_tx_identifier(
  volatile can0_registers_t *can,
  uint16_t value
);
void can0_write_tx_dlc(volatile can0_registers_t *can, uint8_t value);
void can0_write_tx_data(volatile can0_registers_t *can, uint8_t value);
uint16_t can0_read_rx_identifier(volatile can0_registers_t *can);
uint8_t can0_read_rx_dlc(volatile can0_registers_t *can);
uint8_t can0_read_rx_data(volatile can0_registers_t *can);

uint32_t can0_irq_save_disable(void);
void can0_irq_restore(uint32_t state);

#endif

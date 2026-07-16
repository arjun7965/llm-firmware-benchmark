#ifndef FIXTURE_TIMER_DMA_HANDOFF_H
#define FIXTURE_TIMER_DMA_HANDOFF_H

#include <stdint.h>

#define TIMER0_BASE_ADDRESS UINT32_C(0x40012000)
#define DMA0_BASE_ADDRESS UINT32_C(0x40021000)

#define TIMER0_MAX_PERIOD_TICKS UINT16_C(1024)
#define DMA0_MAX_TRANSFER_COUNT UINT32_C(8)

#define TIMER0_CONTROL_ENABLE UINT32_C(1)
#define TIMER0_CONTROL_DMA_REQUEST_ENABLE (UINT32_C(1) << 1)
#define TIMER0_CONTROL_READY TIMER0_CONTROL_ENABLE
#define TIMER0_CONTROL_DMA_OWNED \
  (TIMER0_CONTROL_READY | TIMER0_CONTROL_DMA_REQUEST_ENABLE)
#define TIMER0_CONTROL_ALL TIMER0_CONTROL_DMA_OWNED

#define DMA0_CHANNEL_CONTROL_ENABLE UINT32_C(1)
#define DMA0_CHANNEL_CONTROL_SOURCE_INCREMENT (UINT32_C(1) << 1)
#define DMA0_CHANNEL_CONTROL_COMPLETE_IRQ_ENABLE (UINT32_C(1) << 2)
#define DMA0_CHANNEL_CONTROL_ERROR_IRQ_ENABLE (UINT32_C(1) << 3)
#define DMA0_CHANNEL_CONTROL_ABORT_IRQ_ENABLE (UINT32_C(1) << 4)
#define DMA0_CHANNEL_CONTROL_ABORT (UINT32_C(1) << 5)
#define DMA0_CHANNEL_CONTROL_READY \
  (DMA0_CHANNEL_CONTROL_ENABLE | \
    DMA0_CHANNEL_CONTROL_SOURCE_INCREMENT | \
    DMA0_CHANNEL_CONTROL_COMPLETE_IRQ_ENABLE | \
    DMA0_CHANNEL_CONTROL_ERROR_IRQ_ENABLE | \
    DMA0_CHANNEL_CONTROL_ABORT_IRQ_ENABLE)
#define DMA0_CHANNEL_CONTROL_ALL \
  (DMA0_CHANNEL_CONTROL_READY | DMA0_CHANNEL_CONTROL_ABORT)

#define DMA0_STATUS_COMPLETE UINT32_C(1)
#define DMA0_STATUS_ERROR (UINT32_C(1) << 1)
#define DMA0_STATUS_ABORTED (UINT32_C(1) << 2)
#define DMA0_STATUS_ALL \
  (DMA0_STATUS_COMPLETE | DMA0_STATUS_ERROR | DMA0_STATUS_ABORTED)

typedef struct timer0_registers timer0_registers_t;
typedef struct dma0_registers dma0_registers_t;

uintptr_t timer_dma_buffer_address(const void *buffer);
uintptr_t timer0_compare_dma_address(const volatile timer0_registers_t *timer);

void timer0_write_control(
  volatile timer0_registers_t *timer,
  uint32_t value
);
void timer0_write_period(volatile timer0_registers_t *timer, uint16_t value);
void timer0_write_compare_shadow(
  volatile timer0_registers_t *timer,
  uint16_t value
);
uint16_t timer0_read_compare_active(
  const volatile timer0_registers_t *timer
);

void dma0_write_source(volatile dma0_registers_t *dma, uintptr_t value);
void dma0_write_destination(volatile dma0_registers_t *dma, uintptr_t value);
void dma0_write_count(volatile dma0_registers_t *dma, uint32_t value);
void dma0_write_control(volatile dma0_registers_t *dma, uint32_t value);
uint32_t dma0_read_status(const volatile dma0_registers_t *dma);
void dma0_write_status_clear(volatile dma0_registers_t *dma, uint32_t value);

uint32_t timer_dma_irq_save_disable(void);
void timer_dma_irq_restore(uint32_t state);

#endif

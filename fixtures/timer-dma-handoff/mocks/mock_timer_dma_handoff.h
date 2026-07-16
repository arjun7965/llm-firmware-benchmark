#ifndef MOCK_TIMER_DMA_HANDOFF_H
#define MOCK_TIMER_DMA_HANDOFF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_timer_dma_handoff.h"

typedef enum {
  MOCK_TIMER_DMA_EVENT_TIMER_CONTROL_WRITE,
  MOCK_TIMER_DMA_EVENT_TIMER_PERIOD_WRITE,
  MOCK_TIMER_DMA_EVENT_TIMER_COMPARE_SHADOW_WRITE,
  MOCK_TIMER_DMA_EVENT_TIMER_COMPARE_ACTIVE_READ,
  MOCK_TIMER_DMA_EVENT_DMA_SOURCE_WRITE,
  MOCK_TIMER_DMA_EVENT_DMA_DESTINATION_WRITE,
  MOCK_TIMER_DMA_EVENT_DMA_COUNT_WRITE,
  MOCK_TIMER_DMA_EVENT_DMA_CONTROL_WRITE,
  MOCK_TIMER_DMA_EVENT_DMA_STATUS_READ,
  MOCK_TIMER_DMA_EVENT_DMA_STATUS_CLEAR_WRITE,
  MOCK_TIMER_DMA_EVENT_IRQ_SAVE_DISABLE,
  MOCK_TIMER_DMA_EVENT_IRQ_RESTORE,
  MOCK_TIMER_DMA_EVENT_COUNT,
} mock_timer_dma_event_t;

void mock_timer_dma_reset(void);
volatile timer0_registers_t *mock_timer0(void);
volatile dma0_registers_t *mock_dma0(void);
bool mock_timer_dma_tick(void);
void mock_timer_dma_set_status(uint32_t value);
void mock_timer_dma_set_irq_state(uint32_t value);

uint32_t mock_timer0_control(void);
uint16_t mock_timer0_period(void);
uint16_t mock_timer0_compare_shadow(void);
uint16_t mock_timer0_compare_active(void);
uint32_t mock_dma0_control(void);
uintptr_t mock_dma0_source(void);
uintptr_t mock_dma0_destination(void);
uint32_t mock_dma0_count(void);
uint32_t mock_dma0_transferred(void);
uint32_t mock_dma0_status(void);
uint32_t mock_timer_dma_irq_state(void);
size_t mock_timer_dma_event_count(void);
mock_timer_dma_event_t mock_timer_dma_event_at(size_t index);
uint32_t mock_timer_dma_event_value(size_t index);
bool mock_timer_dma_invalid_access(void);

#endif

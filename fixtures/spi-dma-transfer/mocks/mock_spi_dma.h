#ifndef MOCK_SPI_DMA_H
#define MOCK_SPI_DMA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_spi_dma.h"

#define MOCK_SPI_DMA_RESPONSE_XOR UINT8_C(0xA5)

typedef enum {
  MOCK_SPI_DMA_EVENT_SPI_CONTROL,
  MOCK_SPI_DMA_EVENT_SPI_CLOCK_DIVIDER,
  MOCK_SPI_DMA_EVENT_SPI_CHIP_SELECT,
  MOCK_SPI_DMA_EVENT_DMA_TX_SOURCE,
  MOCK_SPI_DMA_EVENT_DMA_TX_DESTINATION,
  MOCK_SPI_DMA_EVENT_DMA_TX_COUNT,
  MOCK_SPI_DMA_EVENT_DMA_TX_CONTROL,
  MOCK_SPI_DMA_EVENT_DMA_RX_SOURCE,
  MOCK_SPI_DMA_EVENT_DMA_RX_DESTINATION,
  MOCK_SPI_DMA_EVENT_DMA_RX_COUNT,
  MOCK_SPI_DMA_EVENT_DMA_RX_CONTROL,
  MOCK_SPI_DMA_EVENT_DMA_STATUS_CLEAR,
  MOCK_SPI_DMA_EVENT_COUNT,
} mock_spi_dma_event_t;

void mock_spi_dma_reset(void);
volatile spi0_registers_t *mock_spi0(void);
volatile dma0_registers_t *mock_dma0(void);

bool mock_spi_dma_signal_tx_complete(void);
bool mock_spi_dma_signal_rx_complete(void);
void mock_spi_dma_set_status(uint32_t status);

size_t mock_spi_dma_event_count(void);
mock_spi_dma_event_t mock_spi_dma_event_at(size_t index);

uint32_t mock_spi_control(void);
uint32_t mock_spi_control_at(size_t index);
size_t mock_spi_control_write_count(void);
uint32_t mock_spi_clock_divider(void);
size_t mock_spi_clock_divider_write_count(void);
uint32_t mock_spi_chip_select(void);
uint32_t mock_spi_chip_select_at(size_t index);
size_t mock_spi_chip_select_write_count(void);
uintptr_t mock_spi_data_address(void);

uintptr_t mock_dma_tx_source(void);
uintptr_t mock_dma_tx_destination(void);
uint32_t mock_dma_tx_count(void);
uint32_t mock_dma_tx_control(void);
size_t mock_dma_tx_control_write_count(void);
uintptr_t mock_dma_rx_source(void);
uintptr_t mock_dma_rx_destination(void);
uint32_t mock_dma_rx_count(void);
uint32_t mock_dma_rx_control(void);
size_t mock_dma_rx_control_write_count(void);
uint32_t mock_dma_status(void);
size_t mock_dma_status_read_count(void);
size_t mock_dma_status_clear_write_count(void);
uint32_t mock_dma_last_status_clear(void);

void mock_spi_dma_set_interrupts_enabled(bool enabled);
bool mock_spi_dma_interrupts_enabled(void);
size_t mock_spi_dma_irq_save_count(void);
size_t mock_spi_dma_irq_restore_count(void);
uint32_t mock_spi_dma_last_irq_restore(void);

#endif

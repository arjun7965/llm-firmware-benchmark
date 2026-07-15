#ifndef FIXTURE_SPI_DMA_H
#define FIXTURE_SPI_DMA_H

#include <stdint.h>

#define SPI0_BASE_ADDRESS UINT32_C(0x40013000)
#define DMA0_BASE_ADDRESS UINT32_C(0x40020000)

#define SPI0_CONTROL_ENABLE UINT32_C(1)
#define SPI0_CONTROL_MASTER (UINT32_C(1) << 1)
#define SPI0_CONTROL_TX_DMA_ENABLE (UINT32_C(1) << 2)
#define SPI0_CONTROL_RX_DMA_ENABLE (UINT32_C(1) << 3)
#define SPI0_CONTROL_DMA_MASK \
  (SPI0_CONTROL_TX_DMA_ENABLE | SPI0_CONTROL_RX_DMA_ENABLE)

#define SPI0_CHIP_SELECT_ASSERTED UINT32_C(0)
#define SPI0_CHIP_SELECT_DEASSERTED UINT32_C(1)

#define DMA0_CHANNEL_CONTROL_ENABLE UINT32_C(1)
#define DMA0_CHANNEL_CONTROL_SOURCE_INCREMENT (UINT32_C(1) << 1)
#define DMA0_CHANNEL_CONTROL_DESTINATION_INCREMENT (UINT32_C(1) << 2)
#define DMA0_CHANNEL_CONTROL_COMPLETE_IRQ_ENABLE (UINT32_C(1) << 3)
#define DMA0_CHANNEL_CONTROL_ERROR_IRQ_ENABLE (UINT32_C(1) << 4)

#define DMA0_STATUS_TX_COMPLETE UINT32_C(1)
#define DMA0_STATUS_RX_COMPLETE (UINT32_C(1) << 1)
#define DMA0_STATUS_TX_ERROR (UINT32_C(1) << 2)
#define DMA0_STATUS_RX_ERROR (UINT32_C(1) << 3)
#define DMA0_STATUS_COMPLETE_MASK \
  (DMA0_STATUS_TX_COMPLETE | DMA0_STATUS_RX_COMPLETE)
#define DMA0_STATUS_ERROR_MASK \
  (DMA0_STATUS_TX_ERROR | DMA0_STATUS_RX_ERROR)
#define DMA0_STATUS_ALL \
  (DMA0_STATUS_COMPLETE_MASK | DMA0_STATUS_ERROR_MASK)

typedef struct spi0_registers spi0_registers_t;
typedef struct dma0_registers dma0_registers_t;

uintptr_t spi_dma_buffer_address(const void *buffer);
uintptr_t spi0_data_dma_address(const volatile spi0_registers_t *spi);

void spi0_write_control(
  volatile spi0_registers_t *spi,
  uint32_t value
);
void spi0_write_clock_divider(
  volatile spi0_registers_t *spi,
  uint32_t value
);
void spi0_write_chip_select(
  volatile spi0_registers_t *spi,
  uint32_t value
);

void dma0_write_tx_source(
  volatile dma0_registers_t *dma,
  uintptr_t value
);
void dma0_write_tx_destination(
  volatile dma0_registers_t *dma,
  uintptr_t value
);
void dma0_write_tx_count(
  volatile dma0_registers_t *dma,
  uint32_t value
);
void dma0_write_tx_control(
  volatile dma0_registers_t *dma,
  uint32_t value
);
void dma0_write_rx_source(
  volatile dma0_registers_t *dma,
  uintptr_t value
);
void dma0_write_rx_destination(
  volatile dma0_registers_t *dma,
  uintptr_t value
);
void dma0_write_rx_count(
  volatile dma0_registers_t *dma,
  uint32_t value
);
void dma0_write_rx_control(
  volatile dma0_registers_t *dma,
  uint32_t value
);
uint32_t dma0_read_status(const volatile dma0_registers_t *dma);
void dma0_write_status_clear(
  volatile dma0_registers_t *dma,
  uint32_t value
);

uint32_t spi_dma_irq_save_disable(void);
void spi_dma_irq_restore(uint32_t state);

#endif

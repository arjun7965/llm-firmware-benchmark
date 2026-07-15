#ifndef SPI_DMA_DRIVER_H
#define SPI_DMA_DRIVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_spi_dma.h"

#define SPI_DMA_MAX_TRANSFER UINT32_C(32)
#define SPI_DMA_CLOCK_DIVIDER UINT32_C(8)

typedef enum {
  SPI_DMA_RESULT_NONE = 0,
  SPI_DMA_RESULT_COMPLETE,
  SPI_DMA_RESULT_ERROR,
} spi_dma_result_t;

typedef struct {
  volatile spi0_registers_t *spi;
  volatile dma0_registers_t *dma;
  const uint8_t *tx_buffer;
  uint8_t *rx_buffer;
  size_t transfer_length;
  uint32_t error_flags;
  spi_dma_result_t result;
  bool tx_complete;
  bool rx_complete;
  bool busy;
  bool initialized;
} spi_dma_driver_t;

_Static_assert(
  SPI_DMA_MAX_TRANSFER > 0u,
  "SPI DMA transfers must have a nonzero bound"
);

bool spi_dma_init(
  spi_dma_driver_t *driver,
  volatile spi0_registers_t *spi,
  volatile dma0_registers_t *dma
);
bool spi_dma_start(
  spi_dma_driver_t *driver,
  const uint8_t *tx,
  uint8_t *rx,
  size_t length
);
void spi_dma_irq(spi_dma_driver_t *driver);
bool spi_dma_is_busy(spi_dma_driver_t *driver);
spi_dma_result_t spi_dma_take_result(
  spi_dma_driver_t *driver,
  uint32_t *error_flags
);

#endif

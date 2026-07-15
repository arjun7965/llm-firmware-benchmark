#include "spi_dma_driver.h"

#define SPI0_CONTROL_IDLE (SPI0_CONTROL_ENABLE | SPI0_CONTROL_MASTER)
#define DMA0_TX_CONTROL \
  (DMA0_CHANNEL_CONTROL_ENABLE | \
    DMA0_CHANNEL_CONTROL_SOURCE_INCREMENT | \
    DMA0_CHANNEL_CONTROL_COMPLETE_IRQ_ENABLE | \
    DMA0_CHANNEL_CONTROL_ERROR_IRQ_ENABLE)
#define DMA0_RX_CONTROL \
  (DMA0_CHANNEL_CONTROL_ENABLE | \
    DMA0_CHANNEL_CONTROL_DESTINATION_INCREMENT | \
    DMA0_CHANNEL_CONTROL_COMPLETE_IRQ_ENABLE | \
    DMA0_CHANNEL_CONTROL_ERROR_IRQ_ENABLE)

static bool driver_is_ready(const spi_dma_driver_t *driver) {
  return driver != NULL && driver->initialized && driver->spi != NULL &&
    driver->dma != NULL;
}

static void disable_dma_channels(spi_dma_driver_t *driver) {
  dma0_write_tx_control(driver->dma, 0u);
  dma0_write_rx_control(driver->dma, 0u);
}

static void clear_active_transfer(spi_dma_driver_t *driver) {
  driver->tx_buffer = NULL;
  driver->rx_buffer = NULL;
  driver->transfer_length = 0u;
  driver->tx_complete = false;
  driver->rx_complete = false;
  driver->busy = false;
}

static void finish_transfer(
  spi_dma_driver_t *driver,
  spi_dma_result_t result
) {
  spi0_write_control(driver->spi, SPI0_CONTROL_IDLE);
  disable_dma_channels(driver);
  spi0_write_chip_select(driver->spi, SPI0_CHIP_SELECT_DEASSERTED);
  clear_active_transfer(driver);
  driver->result = result;
}

bool spi_dma_init(
  spi_dma_driver_t *driver,
  volatile spi0_registers_t *spi,
  volatile dma0_registers_t *dma
) {
  if (driver == NULL || spi == NULL || dma == NULL) return false;

  *driver = (spi_dma_driver_t){ 0 };
  driver->spi = spi;
  driver->dma = dma;
  spi0_write_control(spi, 0u);
  disable_dma_channels(driver);
  dma0_write_status_clear(dma, DMA0_STATUS_ALL);
  spi0_write_clock_divider(spi, SPI_DMA_CLOCK_DIVIDER);
  spi0_write_chip_select(spi, SPI0_CHIP_SELECT_DEASSERTED);
  spi0_write_control(spi, SPI0_CONTROL_IDLE);
  driver->initialized = true;
  return true;
}

bool spi_dma_start(
  spi_dma_driver_t *driver,
  const uint8_t *tx,
  uint8_t *rx,
  size_t length
) {
  if (
    !driver_is_ready(driver) || tx == NULL || rx == NULL || length == 0u ||
    length > SPI_DMA_MAX_TRANSFER
  ) {
    return false;
  }

  const uint32_t start_interrupt_state = spi_dma_irq_save_disable();
  bool started = false;
  if (!driver->busy && driver->result == SPI_DMA_RESULT_NONE) {
    driver->tx_buffer = tx;
    driver->rx_buffer = rx;
    driver->transfer_length = length;
    driver->error_flags = 0u;
    driver->tx_complete = false;
    driver->rx_complete = false;
    driver->busy = true;

    dma0_write_status_clear(driver->dma, DMA0_STATUS_ALL);
    spi0_write_chip_select(driver->spi, SPI0_CHIP_SELECT_ASSERTED);
    dma0_write_rx_source(
      driver->dma,
      spi0_data_dma_address(driver->spi)
    );
    dma0_write_rx_destination(
      driver->dma,
      spi_dma_buffer_address(rx)
    );
    dma0_write_rx_count(driver->dma, (uint32_t)length);
    dma0_write_rx_control(driver->dma, DMA0_RX_CONTROL);
    dma0_write_tx_source(
      driver->dma,
      spi_dma_buffer_address(tx)
    );
    dma0_write_tx_destination(
      driver->dma,
      spi0_data_dma_address(driver->spi)
    );
    dma0_write_tx_count(driver->dma, (uint32_t)length);
    dma0_write_tx_control(driver->dma, DMA0_TX_CONTROL);
    spi0_write_control(
      driver->spi,
      SPI0_CONTROL_IDLE | SPI0_CONTROL_DMA_MASK
    );
    started = true;
  }
  spi_dma_irq_restore(start_interrupt_state);
  return started;
}

void spi_dma_irq(spi_dma_driver_t *driver) {
  if (!driver_is_ready(driver) || !driver->busy) return;

  const uint32_t observed = dma0_read_status(driver->dma) & DMA0_STATUS_ALL;
  if (observed == 0u) return;

  dma0_write_status_clear(driver->dma, observed);
  const uint32_t errors = observed & DMA0_STATUS_ERROR_MASK;
  if (errors != 0u) {
    driver->error_flags |= errors;
    finish_transfer(driver, SPI_DMA_RESULT_ERROR);
    return;
  }
  if ((observed & DMA0_STATUS_TX_COMPLETE) != 0u) {
    driver->tx_complete = true;
  }
  if ((observed & DMA0_STATUS_RX_COMPLETE) != 0u) {
    driver->rx_complete = true;
  }
  if (driver->tx_complete && driver->rx_complete) {
    finish_transfer(driver, SPI_DMA_RESULT_COMPLETE);
  }
}

bool spi_dma_is_busy(spi_dma_driver_t *driver) {
  if (!driver_is_ready(driver)) return false;

  const uint32_t busy_interrupt_state = spi_dma_irq_save_disable();
  const bool busy = driver->busy;
  spi_dma_irq_restore(busy_interrupt_state);
  return busy;
}

spi_dma_result_t spi_dma_take_result(
  spi_dma_driver_t *driver,
  uint32_t *error_flags
) {
  if (!driver_is_ready(driver) || error_flags == NULL) {
    return SPI_DMA_RESULT_NONE;
  }

  const uint32_t result_interrupt_state = spi_dma_irq_save_disable();
  const spi_dma_result_t result = driver->result;
  if (result == SPI_DMA_RESULT_NONE) {
    *error_flags = 0u;
  } else {
    *error_flags = driver->error_flags;
    driver->error_flags = 0u;
    driver->result = SPI_DMA_RESULT_NONE;
  }
  spi_dma_irq_restore(result_interrupt_state);
  return result;
}

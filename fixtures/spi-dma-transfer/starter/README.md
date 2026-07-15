# SPI DMA Transfer Starter API

Implement spi_dma_driver.h using only the accessors in fixture_spi_dma.h. The
caller owns spi_dma_driver_t and keeps its TX and RX buffers valid until the
driver reports a terminal result. The fixture owns the opaque SPI0 and DMA0
register models.

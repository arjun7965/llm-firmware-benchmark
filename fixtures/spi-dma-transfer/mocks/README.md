# SPI0 and DMA0 Mock

mock_spi_dma.c models opaque SPI0 and DMA0 register banks, write-one-to-clear
DMA status, full-duplex DMA data movement, and an interrupt-mask boundary.
Tests manually signal TX/RX completion or error status; no host threads,
physical SPI peripheral, or DMA hardware is used.

# DMA-Aware SPI Transfer

## Objective

Assess a full-duplex SPI driver that configures a paired DMA transaction, keeps
caller-owned buffers stable while hardware owns them, and turns separate DMA
completion or error IRQs into one recoverable terminal result.

Implement the API declared by
`fixtures/spi-dma-transfer/starter/spi_dma_driver.h` against the supplied
opaque SPI0/DMA0 register accessors and interrupt-mask boundary.

## Target Assumptions

Target profile: `armv7m-bare-metal`. The task uses a single-core little-endian
Cortex-M3 with AAPCS/EABI, no heap, cache, FPU, RTOS, vendor SDK, or host
threads. DMA addresses are supplied by fixture accessors; buffers are
caller-owned, nonoverlapping, DMA-visible, and remain unchanged through
completion. The DMA IRQ is non-nested and may interrupt foreground calls, so
foreground state access preserves the exact save-disable state. The Cortex-M3
has no data cache, so cache-maintenance APIs are intentionally out of scope.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Initialization follows the declared hardware order; start maps full-duplex TX/RX descriptors correctly; a result becomes complete only after both channels report completion.
- 1 point — **Bounded resource use:** The answer uses only caller-owned state and buffers, has a maximum 32-byte transfer, and introduces no allocation, polling, retry, unbounded loop, or mutable global state.
- 2 points — **Timing behavior:** RX DMA is configured before TX DMA, SPI DMA requests are enabled only after both descriptors are ready, and the ISR takes one status snapshot with no waiting.
- 2 points — **Concurrency safety:** Foreground start/result/busy operations preserve the exact interrupt state, reject overlap and unconsumed results, and the non-nested ISR never changes that state.
- 1 point — **Fault recovery:** Stale and observed DMA status are acknowledged, an error wins over simultaneous completion, all terminal paths disable DMA and deassert chip select, and a taken error permits recovery.
- 1 point — **Portability:** The implementation uses portable freestanding C11, fixed-width and pointer-width integer types only through the supplied address accessors, and no vendor-specific DMA API.
- 1 point — **Clarity and validation:** The explanation states DMA ownership, completion/error state transitions, and ordering; tests cover initialization, capacity, descriptor direction, split completion, error priority, and interrupt restoration.

Direct CPU writes to SPI data, configuring TX before RX, polling for DMA status,
leaving DMA request lines enabled after a terminal IRQ, or restoring interrupts
as enabled unconditionally do not receive the corresponding timing or
concurrency-safety credit.

# DMA Cache Coherency

## Objective

Assess deterministic cache-maintenance ordering around a noncoherent DMA channel, including cache-line range expansion, buffer alignment, receive state, and terminal error cleanup.

Implement the API declared by `fixtures/dma-cache-coherency/starter/dma_cache_transfer.h` using only the fixture-provided cache and DMA functions.

## Target Assumptions

Target profile: `armv7m-bare-metal`. This task explicitly selects a fictional single-core little-endian Cortex-M7 override with a 32-byte noncoherent data cache, one receive transfer slot, four-byte DMA-buffer alignment, and opaque fixture-owned cache/DMA calls. Cache calls record ranges but do not dereference them; no vendor cache or DMA register access is permitted.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Valid initialization, exact cache-line range construction, transmit launch, receive launch, busy retry, and completion state transitions are correct.
- 1 point — **Bounded resource use:** The answer uses the caller-owned transfer state and supplied fixed channel with no allocation, polling loop, retry loop, or mutable global state.
- 2 points — **Timing behavior:** Clean occurs before transmit launch; receive invalidation occurs before launch and after only a successful terminal completion; busy performs no extra cache operation.
- 1 point — **Concurrency safety:** One receive slot cannot be overwritten while in flight, and state is published only after a successful receive start before later completion handling.
- 2 points — **Fault recovery:** Invalid range/alignment and uninitialized calls make no fixture call; start failures do not publish receive state; terminal receive failures clear it without falsely invalidating data.
- 1 point — **Portability:** C11 integer range arithmetic handles alignment and overflow without direct cache registers, inline assembly, vendor APIs, or dereferencing rounded pointers.
- 1 point — **Clarity and validation:** The explanation states cache-line rounding, TX/RX ordering, busy/error cleanup, and deterministic mock-based tests.

Cleaning after DMA start, failing to invalidate both receive boundaries, using the rounded cache address as the DMA address, or dropping receive state on a busy result cannot receive the relevant timing, correctness, or recovery credit.

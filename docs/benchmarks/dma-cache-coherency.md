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

## Calibration

The September 19, 2026 pilot uses a prompt containing both exact supplied
headers. It explicitly defines caller-owned initialized state, serialized API
calls, independent TX while RX is pending, and opaque address recording without
dereferencing synthetic boundary addresses.

Before generation, the trusted reference passed six public test groups under
`c11-host` revision 4, Debian 13 x86-64, GCC 14.2.0, and Bubblewrap 0.11.0.
All 13 compile-valid mutations were rejected. Added checks cover exact cache-line
endpoints, maximum length, buffer-end and cache-rounding overflow, the last
representable range, status propagation, pending-receive state preservation,
and reinitialization. These checks extend the previous four-group, six-mutation
fixture before any model answers are collected.

The initial nine-attempt cohort exposed an underspecified overlapping-receive
return status: seven compiled answers returned `BUSY`, whereas the fixture
required `INVALID_ARGUMENT`. Another compiled answer assumed `uintmax_t` was
wider than `uintptr_t`; the nominal target is 32-bit but the validation host
uses 64-bit types for both. One final-message answer was an incomplete source
fragment and failed compilation. These records remain unscored diagnostic
evidence with identities sealed.

The revised prompt explicitly defines the overlap status and host integer
widths. A fresh prospective cohort scheduled three attempts each from GPT-5.6
Luna, GLM-5.3, and Kimi K3 with unchanged provider budgets, fixture, and rubric.
All nine revised attempts are recorded, and AI criterion scores were frozen
before unblinding. The user approved those scores and publication without changes
on September 19, 2026 (America/Los_Angeles). The two cohorts are not pooled.

All nine revised answers extracted; eight compiled and passed all six test
groups. GLM run 3 failed compilation because a comment contained a nested
comment opener under the declared warning-as-error flags. No candidate was
repaired. Reviewed means are Luna 9.500, GLM 9.000, and Kimi 9.833 out of 10;
full validation passes are 3/3, 2/3, and 3/3, respectively.

See the [reviewed results and methodology](../model-family-calibration.md#completed-dma-cache-pilot--dma-cache-coherency),
[sanitized aggregate](../calibration/dma-cache-coherency-2026-09-19.json),
and [calibration plot](../calibration/dma-cache-coherency-2026-09-19.svg).
Scoring is AI-assisted and human-reviewed, not independent human scoring.

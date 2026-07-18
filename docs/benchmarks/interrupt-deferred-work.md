# Interrupt Deferred Work Across Nested IRQs

## Objective

Assess a deterministic ISR-to-foreground deferred-work dispatcher that safely
coalesces work from two priority-ordered interrupt sources without treating
`volatile` as synchronization.

Implement the API declared by
`fixtures/interrupt-deferred-work/starter/deferred_work.h` using only its
opaque latch and interrupt-mask accessors.

## Target Assumptions

Target profile: `armv7m-bare-metal`. The task models a single-core,
little-endian Cortex-M3 using AAPCS/EABI and lock-free C11
`atomic_uint_fast32_t` operations. A high-priority IRQ may preempt a
low-priority IRQ, but neither can reenter itself. Foreground save-disable masks
both IRQs. The latch address map is intentionally opaque; compiler-generated
exclusive accesses are allowed only through C11 atomics. The opaque volatile
latch is MMIO only; all shared software work state uses C11 atomics. There is
no heap, cache, DMA, FPU, RTOS, vendor SDK, or host thread.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Initialization and source
  reconfiguration use the exact latch-write order; each IRQ acknowledges only
  its own observed source and deferred work coalesces by source bit.
- 1 point — **Bounded resource use:** The answer uses caller-owned fixed-size
  state with no allocation, queue growth, polling, retry, callback, or mutable
  global state.
- 2 points — **Timing behavior:** Each ISR takes one latch snapshot and has
  bounded work; repeated IRQs defer a single later foreground service rather
  than executing work in interrupt context.
- 2 points — **Concurrency safety:** C11 atomic release fetch-or and
  acquire-release exchange preserve both sources across a nested high/low IRQ
  interleaving; foreground configuration preserves the exact global interrupt
  state and never discards an already deferred bit.
- 1 point — **Fault recovery:** Source reconfiguration clears stale latch bits
  before re-enabling delivery without discarding already deferred work; invalid
  calls have no latch or mask effects; ISR-safe entry points never mask
  interrupts or touch foreground-only non-atomic state.
- 1 point — **Portability:** The implementation is freestanding C11, uses only
  fixed-width types and supplied accessors, and has no direct register access,
  casts, inline assembly, mutex, or vendor dependency.
- 1 point — **Clarity and validation:** The explanation distinguishes MMIO
  volatility from atomic synchronization and covers the deterministic nested
  IRQ, stale-latch, coalescing, configuration, and atomic-claim tests.

Using a non-atomic read-modify-write that can overwrite a nested IRQ's source,
clearing another source's latch bit, enabling sources while reconfiguring, or
using `volatile` for software synchronization does not receive the relevant
correctness, race-safety, or concurrency credit.

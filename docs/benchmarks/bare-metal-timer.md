# Bare-Metal Timer

## Objective

Assess deterministic MMIO configuration, register-write ordering, validation,
and interrupt acknowledgement for a fully specified fictional timer.

Implement the API declared by
`fixtures/bare-metal-timer/starter/fictional_timer.h`. The host fixture
instruments the supplied accessors; a target build uses direct volatile MMIO.

## Target Assumptions

Target profile: `armv7m-bare-metal`. The task uses a single-core little-endian
Cortex-M3 with AAPCS/EABI, privileged execution, masked interrupts during
configuration, no nested interrupts, and no heap, cache, DMA, FPU, or RTOS.

## Scoring

Scoring profile: `firmware-v1`.

- 3 points — **Functional correctness:** Valid clock and period inputs produce the specified prescaler, reload, control, and stop behavior.
- 1 point — **Bounded resource use:** The implementation uses no allocation, mutable global state, or unbounded loop.
- 1 point — **Timing behavior:** Configuration follows the required disable, program, clear, then enable write sequence.
- 1 point — **Concurrency safety:** Supplied volatile accessors implement pending queries and write-one-to-clear acknowledgement without unsafe read-modify-write behavior.
- 1 point — **Fault recovery:** Every invalid or null input follows its specified result and invalid configuration performs no MMIO writes.
- 2 points — **Portability:** Fixed-width arithmetic, register layout, and the ARMv7-M MMIO contract are handled without overflow, extensions, or undefined behavior.
- 1 point — **Clarity and validation:** Calculations and assumptions are reviewable and focused tests cover ordering, boundaries, rejection, stop, and interrupt handling.

Direct register access that bypasses the supplied accessors receives no
concurrency-safety credit. Do not also deduct timing points solely because those
writes are unobservable to the host hook.

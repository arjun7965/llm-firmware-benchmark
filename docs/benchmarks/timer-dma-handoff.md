# Timer-DMA Ownership Handoff

## Objective

Assess a bounded timer driver that explicitly hands compare-stream ownership to
DMA, returns it deterministically on terminal completion, and preserves the
last hardware-applied compare value through abort or error recovery.

Implement the API declared by
`fixtures/timer-dma-handoff/starter/timer_dma_handoff.h` against the supplied
opaque TIMER0, DMA0, and interrupt-mask accessors.

## Target Assumptions

Target profile: `armv7m-bare-metal`. The fixture models a single-core
little-endian Cortex-M3 with AAPCS/EABI, no heap, cache, FPU, RTOS, vendor SDK,
or host threads. TIMER0 has a period and compare shadow; DMA0 writes a bounded
caller-owned compare sequence at deterministic timer boundaries. While DMA
owns the stream, foreground code must not reconfigure the compare output. An
abort only becomes terminal when DMA latches `ABORTED`; the retained active
compare then seeds explicit CPU-owned recovery. Foreground operations use the
fixture-owned interrupt save/restore boundary and must restore the exact prior
state. The DMA IRQ is non-nested and runs without foreground interrupt-mask
calls.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Initialization validates timer bounds
  and follows the declared programming order; a start programs a bounded DMA
  descriptor and reports completion, abort, and error results correctly.
- 1 point — **Bounded resource use:** The implementation uses caller-owned
  state, validates at most eight samples, and has no allocation, busy wait,
  retry loop, or unbounded work.
- 2 points — **Timing behavior:** DMA receives ownership only after a
  valid descriptor is enabled; terminal handling disables timer DMA requests
  before CPU ownership resumes and retains the exact active compare value.
- 1 point — **Concurrency safety:** Every valid foreground operation preserves
  the exact caller interrupt state and updates shared ownership/result state
  under that boundary, while the non-nested IRQ takes one status snapshot.
- 2 points — **Fault recovery:** Error wins over abort and complete;
  abort/error recovery requires terminal-result acknowledgement, replays the
  exact safe configuration order, and blocks a new handoff until recovery.
- 1 point — **Portability:** The answer uses freestanding C11 and only
  fixture-owned accessors, with no direct register access, pointer casts, or
  vendor-specific APIs.
- 1 point — **Clarity and validation:** The explanation covers DMA ownership,
  terminal priority, retained-compare recovery, interrupt restoration, and
  deterministic boundary tests.

Enabling timer DMA requests before a ready channel, writing the timer while DMA
owns it, treating an abort request as completed before its status IRQ, or
recovering before result acknowledgement are substantial safety defects.

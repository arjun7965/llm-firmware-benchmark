# PWM Synchronized Update

## Objective

Assess a bounded PWM driver that publishes duty changes only at a timer-period
boundary, uses opaque shadow/load accessors rather than direct registers, and
recovers a safe output deterministically after a hardware fault.

Implement the API declared by
`fixtures/pwm-synchronized-update/starter/pwm_update.h` against the supplied
opaque PWM0 and interrupt-mask accessors.

## Target Assumptions

Target profile: `armv7m-bare-metal`. The fixture models a single-core
little-endian Cortex-M3 with AAPCS/EABI, no heap, cache, FPU, RTOS, vendor SDK,
DMA, or host threads. PWM0 latches shadow values immediately only while it is
disabled; while enabled, a selected shadow load reaches the active waveform at
the next controlled period boundary and raises an update latch. Its ISR is
non-nested. Foreground requests, recovery, and event consumption use the
fixture-owned interrupt save/restore boundary to coordinate with that ISR.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Initialization validates period and
  duty and uses the declared programming order; request, applied-event
  reporting, stale-update acknowledgement, and event consumption are correct.
- 1 point — **Bounded resource use:** The implementation uses only
  caller-owned state, performs bounded accessor calls, and has no allocation,
  busy wait, retry loop, or unbounded work.
- 2 points — **Timing behavior:** A requested duty is written through the
  compare shadow and load register, never reported active before the modeled
  period boundary, and stale UPDATE is cleared before a new request is armed.
- 1 point — **Concurrency safety:** Every valid foreground operation preserves
  the exact caller interrupt state, rejects pending or unconsumed work under
  that boundary, and the non-nested ISR takes one status snapshot without
  changing interrupt state.
- 2 points — **Fault recovery:** FAULT wins over simultaneous UPDATE, disables
  PWM output before clearing latches, blocks later requests, and recovery
  restores the stored last known-safe active duty while preserving the fault
  event for consumption.
- 1 point — **Portability:** The answer uses freestanding C11 and only
  fixture-owned accessors, with no direct register access, pointer casts, or
  vendor-specific APIs.
- 1 point — **Clarity and validation:** The explanation covers shadow timing,
  fault recovery, interrupt restoration, and deterministic tests.

Direct active-register writes, publishing a request before its boundary,
allowing an update before a terminal event is consumed, restoring interrupts as
unconditionally enabled, or restarting a faulted output without explicit
recovery are substantial correctness defects.

# Watchdog-Window Recovery

## Objective

Assess a bounded watchdog driver that feeds only inside a configured hardware
window, treats a latched watchdog reset as a terminal safety event, and restores
the same configuration only after explicit reset acknowledgement.

Implement the API declared by
`fixtures/watchdog-window-recovery/starter/watchdog_window.h` against the
supplied opaque WDT0 and interrupt-mask accessors.

## Target Assumptions

Target profile: `armv7m-bare-metal`. The fixture models a single-core
little-endian Cortex-M3 with AAPCS/EABI, no heap, cache, FPU, RTOS, vendor SDK,
DMA, or host threads. WDT0 owns a deterministic tick counter: it accepts the
declared feed key only once its configured window opens and before its timeout;
expiry or an early hardware feed latches RESET, disables WDT0, and restarts the
counter. The reset-cause latch persists until it is explicitly cleared during
boot or retained-state recovery. Foreground driver operations use the
fixture-owned interrupt save/restore boundary to protect their state and must
restore the exact prior state.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Initialization validates timeout and
  window bounds, programs WDT0 in the declared order, and reports feed, reset,
  poll, boot, and event-latch results correctly.
- 1 point — **Bounded resource use:** The implementation uses only
  caller-owned state, performs bounded accessor calls, and has no allocation,
  busy wait, retry loop, or unbounded work.
- 2 points — **Timing behavior:** It rejects feeds before the exact window-open
  boundary without writing the key, accepts the boundary itself, and never
  attempts to feed after the hardware reset latch is observed.
- 1 point — **Concurrency safety:** Every valid foreground operation preserves
  the exact caller interrupt state and updates reset/event state under that
  boundary without racing foreground consumers.
- 2 points — **Fault recovery:** RESET has priority over normal feed gating;
  recovery requires the detected-reset event to be consumed, verifies the
  retained reset cause, replays the exact configuration order, and requires the
  recovery event to be consumed before later feeds.
- 1 point — **Portability:** The answer uses freestanding C11 and only
  fixture-owned accessors, with no direct register access, pointer casts, or
  vendor-specific APIs.
- 1 point — **Clarity and validation:** The explanation covers window safety,
  reset acknowledgement and recovery, interrupt restoration, and deterministic
  boundary tests.

Writing the feed key before the window opens, clearing a reset cause before it
is acknowledged, re-enabling a reset driver without recovery, or restoring
interrupts as unconditionally enabled are substantial safety defects.

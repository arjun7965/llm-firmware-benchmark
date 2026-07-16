# Timer Capture/Compare Overflow Handoff

## Objective

Assess a bounded TIMER1 driver that reconstructs 32-bit capture and compare
timestamps from a free-running 16-bit counter while an overflow interrupt may
be pending at the same time as an edge or compare event.

Implement the API declared by
`fixtures/timer-capture-overflow/starter/timer_capture.h` against the supplied
opaque TIMER1 and interrupt-mask accessors.

## Target Assumptions

Target profile: `armv7m-bare-metal`. The fixture models a single-core
little-endian Cortex-M3 with AAPCS/EABI, no heap, cache, FPU, RTOS, vendor SDK,
DMA, or host threads. TIMER1 is a free-running 16-bit counter. A delayed IRQ
can observe at most one unprocessed wrap. A simultaneous low capture count is
after the observed wrap; a high capture count is before it. Foreground calls
that consume or arm shared state preserve the exact caller interrupt state;
the TIMER1 IRQ is non-nested and never masks interrupts itself.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Initialization follows the exact
  safe order; captures reconstruct timestamps correctly; a relative compare
  fires once at the right 32-bit deadline.
- 1 point — **Bounded resource use:** The implementation uses caller-owned
  state, retains at most one capture, and has no allocation, busy wait, retry,
  or unbounded work.
- 2 points — **Timing behavior:** The half-range handoff rule correctly
  distinguishes pre-wrap from post-wrap captures, and a bounded compare is
  programmed from one counter snapshot without an ambiguous full cycle.
- 1 point — **Concurrency safety:** Valid foreground calls preserve the exact
  interrupt state while the non-nested IRQ takes one status snapshot and
  updates shared latches atomically with respect to foreground code.
- 2 points — **Fault recovery:** Pending timer status prevents a new compare
  from using a stale epoch; stale unarmed compare status is acknowledged; and
  later captures are counted instead of overwriting an unconsumed sample.
- 1 point — **Portability:** The answer is freestanding C11 and uses only the
  supplied accessors, fixed-width arithmetic, and defined unsigned wraparound.
- 1 point — **Clarity and validation:** The explanation covers overflow
  classification, compare arming, sample retention, interrupt restoration, and
  deterministic wrap-boundary tests.

Ignoring a delayed overflow while deriving a capture timestamp, arming a
compare while a timer status is unacknowledged, or treating stale compare status
as a new deadline result are substantial timing defects.

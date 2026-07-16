# GPIO Edge/Debounce Recovery

## Objective

Assess a bounded GPIO button driver that captures active-low rising and falling
edges in a non-nested ISR, establishes a stable state in foreground code after
a wrap-safe debounce interval, and recovers deterministically after wake-up.

Implement the API declared by
`fixtures/gpio-edge-debounce/starter/gpio_debounce.h` using only the supplied
opaque GPIO0 and interrupt-mask accessors.

## Target Assumptions

Target profile: `armv7m-bare-metal`. The fixture models a single-core
little-endian Cortex-M3 with AAPCS/EABI, no heap, cache, FPU, RTOS, vendor SDK,
or host threads. GPIO0 pin 5 is an active-low button. Its ISR is non-nested;
foreground calls use the supplied global interrupt save/restore boundary to
coordinate with it. Edge latches continue to record configured transitions
while normal IRQ delivery is disabled, and a sleep wake source has its own
write-one-to-clear latch.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Initialization configures both button edges in the required order, an ISR captures only the configured pin, and stable press/release transitions are reported exactly after confirmation.
- 1 point — **Bounded resource use:** The implementation uses only caller-owned state, one status snapshot and one input sample per applicable operation, with no allocation, busy wait, retry loop, or unbounded work.
- 2 points — **Timing behavior:** Debounce uses the stated 20 ms elapsed-time boundary across `uint32_t` wraparound, resamples at expiry, and restarts its interval when the observed level changes during bounce.
- 2 points — **Concurrency safety:** ISR and foreground responsibilities are distinct; foreground operations preserve the exact interrupt state; normal delivery remains masked during debounce; and late latched edges survive until a later ISR can process them.
- 1 point — **Fault recovery:** Sleep arming rejects an incomplete debounce, disables normal delivery before clearing stale latches and enabling wake, and resume disables wake, clears both latches, resamples, and debounces a changed level before re-enabling normal IRQ.
- 1 point — **Portability:** The answer uses freestanding C11 and only fixture-owned GPIO0 and interrupt accessors, without direct registers, pointer casts, inline assembly, or vendor APIs.
- 1 point — **Clarity and validation:** The explanation covers active-low edge selection, deadline/bounce behavior, wake recovery, and deterministic tests for initialization order, stale latches, wraparound, late edges, and interrupt restoration.

Treating the active-low input as active high, omitting a button edge direction,
re-enabling IRQ before debounce completes, clearing a late edge during rearm,
or resuming wake without clearing stale latches does not receive the associated
credit.

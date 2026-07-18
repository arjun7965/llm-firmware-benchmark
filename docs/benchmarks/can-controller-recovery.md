# CAN Controller Bus-Off Recovery

## Objective

Assess a bounded classic-CAN controller that programs one outbound mailbox,
retains one inbound frame, handles terminal IRQ priority, and contains a
bus-off fault until hardware explicitly permits deterministic recovery.

Implement the API declared by
`fixtures/can-controller-recovery/starter/can_controller.h` using only its
opaque CAN0 and interrupt-mask accessors.

## Target Assumptions

Target profile: `armv7m-bare-metal`. The task uses a single-core
little-endian Cortex-M3 with AAPCS/EABI, no heap, cache, FPU, RTOS, vendor SDK,
or host threads. It models classic 11-bit CAN frames of at most eight bytes.
The CAN IRQ is non-nested and may interrupt foreground calls, so foreground
state access preserves the exact save-disable state. Bus-off is an externally
latched terminal condition; the supplied recovery-ready indication represents
the controller's completed recovery delay and is not a polling target.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Initialization follows the exact CAN0 configuration order; valid TX frames program identifier, DLC, and bytes correctly; RX retains one complete frame and counts a drained overflow.
- 1 point — **Bounded resource use:** The answer uses only caller-owned controller/frame storage, fixed eight-byte classic-CAN payloads, and no allocation, unbounded queue, polling loop, retry, or mutable global state.
- 2 points — **Timing behavior:** The ISR takes one status snapshot and never waits; TX is armed only after its full mailbox is programmed; recovery takes one readiness snapshot rather than busy-waiting.
- 2 points — **Concurrency safety:** Foreground send, receive, result, drop-count, and recovery calls preserve the exact interrupt state; a pending TX or unconsumed terminal result blocks reuse; the non-nested ISR never changes global interrupt state.
- 1 point — **Fault recovery:** Observed status is acknowledged exactly once, TX error wins over simultaneous completion, bus-off suppresses simultaneous RX/TX work and disables CAN0, and recovery requires ready-without-bus-off before reconfiguration while preserving the terminal result.
- 1 point — **Portability:** The answer uses freestanding C11, fixed-width types, and only supplied opaque accessors, with no direct register access, pointer casts, inline assembly, or vendor API.
- 1 point — **Clarity and validation:** The explanation covers IRQ priority, mailbox ownership, bus-off containment/recovery, and interrupt-state restoration; tests cover configuration order, frame bounds, terminal-result gating, RX overflow drain, simultaneous events, and recovery gating.

Accepting an identifier above `0x7FF` or DLC above eight, enabling a second TX
before its result is consumed, reporting completion over simultaneous TX error,
leaving CAN0 enabled during bus-off, or recovering while BUS_OFF remains set
does not receive the corresponding correctness, concurrency, or recovery
credit.

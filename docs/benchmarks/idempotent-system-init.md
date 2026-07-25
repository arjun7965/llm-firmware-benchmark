# Idempotent System Initialization

## Objective

Assess reset-time system initialization that is side-effect-free for repeated
same-config calls, fails closed on conflicting reconfiguration, and retains an
explicit safe-mode lifecycle across resets.

Implement `fixtures/idempotent-system-init/starter/idempotent_system_init.h`
against the supplied opaque SYSTEM0 accessors.

## Target Assumptions

Target profile: `armv7m-bare-metal`. A single-core little-endian Cortex-M3
uses caller-zeroed state plus a caller-owned retained record. Boot programming
runs with interrupts already masked; safe-mode, resume, and event operations
restore the caller's exact interrupt state.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Validates configurations/records,
  programs the documented initial sequence, identifies repeat and conflict
  calls, and reports each lifecycle result accurately.
- 1 point — **Bounded resource use:** Uses fixed caller-owned state and bounded
  writes with no allocation, polling, retry, or mutable global state.
- 1 point — **Timing behavior:** Preserves the declared SAFE/clock/mask/READY
  order for initial setup and explicit resume.
- 2 points — **Concurrency safety:** Foreground safe-mode, resume, and event
  operations save/restore exact interrupt state and reject pending transitions.
- 2 points — **Fault recovery:** Repairs corrupt retained state, preserves a
  retained safe boot latch, rejects corrupt-record resume, and requires
  explicit event consumption before recovery.
- 1 point — **Portability:** Uses freestanding C11 and only opaque SYSTEM0
  accessors without direct registers or platform-specific APIs.
- 1 point — **Clarity and validation:** Explains idempotency, conflict policy,
  safe-mode persistence, programming order, and deterministic tests.

Repeating controller writes for an already-ready configuration, accepting a
conflict, or resuming before safe-mode acknowledgement is a substantial safety
defect.

# Brownout Safe Mode

## Objective

Assess brownout containment that uses checksum-protected retained state to
force a load into safe mode, then allows an explicit hysteretic recovery.

Implement `fixtures/brownout-safe-mode/starter/brownout_safe_mode.h` against
the supplied opaque PWR0 accessors.

## Target Assumptions

Target profile: `armv7m-bare-metal`. A single-core little-endian Cortex-M3
uses a caller-owned backup-RAM record, opaque PWR0 brownout/status and supply
accessors, and a load-control output. Boot configuration runs with interrupts
already masked; later foreground operations preserve the exact interrupt state.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Validates configuration and retained
  records, detects either latch- or voltage-driven brownout, and maintains the
  documented event/result transitions.
- 1 point — **Bounded resource use:** Uses caller-owned state with bounded
  accessor calls and no allocation, polling loop, retry, or global state.
- 2 points — **Timing behavior:** Enters containment at the inclusive low
  threshold and resumes only at the inclusive, higher recovery threshold.
- 1 point — **Concurrency safety:** Foreground operations save/restore the
  exact interrupt state and atomically consume lifecycle events.
- 2 points — **Fault recovery:** Forces SAFE before retaining/acknowledging a
  brownout, preserves a safe boot latch, gates recovery on event consumption,
  and protects retained integrity with a checksum.
- 1 point — **Portability:** Uses freestanding C11 and only fixture-owned PWR0
  accessors, without direct register access or vendor dependencies.
- 1 point — **Clarity and validation:** Explains retained-state repair,
  hysteresis, containment ordering, and deterministic boundary tests.

Enabling a load during brownout, clearing a latch before safe containment, or
resuming below the recovery threshold is a substantial safety defect.

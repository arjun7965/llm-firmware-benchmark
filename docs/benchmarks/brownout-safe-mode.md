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

## Calibration Contract

The prompt includes both exact fixture-owned headers and warning-as-error C11
compiler flags. A usable manager has non-null manager, peripheral, and retained
record pointers and is initialized. Boot alone validates configuration and
retained integrity; callers do not mutate initialized state between API calls.

The host fixture observes retained records at accessor boundaries. It requires
SAFE before persisting a new entry, a valid safe record before acknowledging
the latch, and ENABLED before clearing the retained safe flag during recovery.
A resume rejected after status/supply sampling must explicitly write SAFE even
when the load is already safe; early state/event rejection performs no PWR0
access. The prompt enumerates every permitted accessor sequence, including
exactly one load-control write on a sampled resume: SAFE on rejection or
ENABLED on success, never both. Omitting a required redundant SAFE write or
adding one before successful ENABLED loses 0.5 fault-recovery points; these
trace defects do not imply the load was enabled during brownout.
Invalid-record repair precedes boot status sampling. Reset scenarios occur
between completed calls; this model does not establish resilience to torn C
stores, physical power loss, or asynchronous supply changes within a call.

Eight public test groups cover configuration and pointer rejection, isolated
retained-field corruption, latch- and voltage-driven entry, both legal threshold
extremes, pending-event preservation and replacement, hysteretic recovery,
exact interrupt restoration, saturating counts, accessor ordering, and repeated
active and healthy reboots. The mutation catalog includes compile-valid defects
for each retained validity check and for containment and persistence ordering.

Score a defect only in its owning dimension. Record validity, input bounds,
counter saturation, and event values belong to functional correctness;
threshold comparisons belong to timing; interrupt restoration and atomic event
consumption belong to concurrency; containment ordering, retained safe boot,
and recovery gating belong to fault recovery. Unsafe load enable, latch clear
before containment, and bypass of recovery gating receive zero fault-recovery
credit. A threshold comparison defect is deducted under timing only. A compile
or extraction failure receives zero functional credit; other dimensions may
receive evidence-supported static credit. Concrete deterministic test scenarios
are required for full clarity/validation credit. A passing fixture alone does
not establish a score of ten.

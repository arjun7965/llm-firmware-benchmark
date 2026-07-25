# Fault Crash Record

## Objective

Assess a bounded fault handler that captures a verified crash record in retained
memory, contains the system in a safe output state, and requires explicit
operator-style recovery.

Implement `fixtures/fault-crash-record/starter/fault_crash_record.h` against
the supplied opaque fault-controller accessors.

## Target Assumptions

Target profile: `armv7m-bare-metal`. A single-core little-endian Cortex-M3
delivers a non-nested fault handler with a supplied exception frame. Retained
memory survives reset, output containment is modeled by FAULT0 control, and
foreground record access preserves the exact interrupt state.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Captures status/frame fields, keeps a
  valid sequence/checksum record, reports retained and new faults, and clears
  records only through the documented lifecycle.
- 1 point — **Bounded resource use:** Uses only fixed caller-owned records and
  performs bounded handler and foreground work with no allocation or retries.
- 1 point — **Timing behavior:** Contains outputs immediately after the one
  required status snapshot and acknowledges only the captured status bits.
- 2 points — **Concurrency safety:** Keeps ISR work non-nested and bounded,
  while foreground read/clear/take operations preserve exact interrupt state.
- 2 points — **Fault recovery:** Rejects corrupted retained records, boots a
  valid record in SAFE state, gates clear on event consumption, and emits a
  recovery event before subsequent lifecycle actions.
- 1 point — **Portability:** Uses freestanding C11 and opaque fixture accessors
  without direct registers, inline assembly, or vendor APIs.
- 1 point — **Clarity and validation:** Explains record integrity, sequence,
  containment order, retained boot behavior, and deterministic fault tests.

Failing to force SAFE in the handler, trusting a corrupt record, or allowing a
pending fault to be cleared silently is a substantial safety defect.

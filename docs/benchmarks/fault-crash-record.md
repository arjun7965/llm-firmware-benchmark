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

## Calibration Scoring Policy

Freeze criterion scores before opening the identity key. Generation failures
have no rubric score. Extraction or compilation failure receives zero
functional correctness; other dimensions may receive supported static credit.
Passing all executable checks does not automatically award ten points.

Assign each defect to its owning dimension to avoid duplicate deductions:
record fields, checksum generation, sequence arithmetic, and read output belong
to functional correctness; unbounded work to resource use; status snapshot,
SAFE/record/acknowledgement ordering and clear-before-NORMAL to timing;
critical sections and exact restoration to concurrency; retained integrity,
event replacement/consumption and recovery gating to fault recovery; forbidden
APIs or language dependencies to portability. Clarity requires both an accurate
explanation of the modeled persistence boundary and concrete test scenarios.
Use half-point partial credit where some of a dimension is demonstrably met.

## Deterministic Model

The prompt supplies both exact headers and the complete accessor contract.
Ten public test groups cover isolated corruption of each retained field,
wrong magic with a consistent checksum, retained reboots, zero/reserved/single
status bits, sequence wrap and invalid-record restart, replacement of pending
events, rejected reads/clears, successful clear ordering, and exact foreground
interrupt restoration including an already-disabled caller.

The mock snapshots retained fields at accessors to check SAFE-before-record,
record-before-acknowledgement, repair-before-NORMAL and zero-before-NORMAL.
Reset and corruption occur between completed calls. Torn stores, power loss
inside a call, real non-maskable fault preemption, and physical retained-memory
durability are excluded. Masking the modeled IRQ state does not establish
exclusion of actual Cortex-M HardFault or NMI handlers.

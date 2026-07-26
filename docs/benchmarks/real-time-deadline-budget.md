# Real-Time Deadline, Jitter, and Budget Guard

## Objective

Assess a deterministic single-core RTOS timing guard that prioritizes control
over telemetry, derives deadlines from nominal releases, bounds release jitter,
and enforces execution budgets without catch-up storms.

Implement the API declared by
`fixtures/real-time-deadline-budget/starter/real_time_guard.h` against the
supplied deterministic RTOS begin/finish/violation boundary.

## Target Assumptions

Target profile: `generic-rtos`. The fixture models a single-core fixed-priority
RTOS with a high-priority control task and lower-priority telemetry task. It
records only begin, finish, and violation calls—no host threads, vendor task
APIs, wall-clock timing, or physical timer is involved. Every compared tick is
within the unsigned 32-bit half range. A begin or finish callback can fail once
to exercise bounded retry behavior.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Initialization establishes both periodic releases; invalid and inactive calls return their documented statuses without RTOS calls; and a successful start/finish maintains the active-job lifecycle exactly.
- 1 point — **Bounded resource use:** The implementation uses caller-owned state with no allocation, global mutable state, spin, task creation, or unbounded catch-up loop.
- 2 points — **Timing behavior:** Releases use wrap-safe ticks; exact maximum jitter, exact budget, and exact nominal deadline are accepted; only a strictly late jitter, budget, or deadline condition reports the matching violation; and deadlines use nominal release plus relative deadline.
- 2 points — **Concurrency safety:** A live job blocks another dispatch, control is considered before telemetry whenever both are due, and all active state changes occur only after the documented RTOS boundary result.
- 1 point — **Fault recovery:** A failed begin leaves its release due, a failed finish keeps the active job for retry, and each timing violation clears only the affected active job while deliberately collapsing that task's missed release.
- 1 point — **Portability:** The answer uses portable C11 unsigned arithmetic and only the supplied RTOS abstraction, without signed-overflow time comparisons or vendor RTOS dependencies.
- 1 point — **Clarity and validation:** The explanation covers fixed-priority release order, nominal versus actual timing, jitter/budget/deadline boundaries, wraparound, and deterministic failure tests.

Do not award timing credit for using `now + deadline`, rejecting an exact
boundary, rescheduling from a stale release, using a non-wrapping comparison,
or reclassifying a deadline miss as an execution-budget violation.

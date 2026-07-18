# RTOS Periodic Scheduler

## Objective

Assess a deterministic rate-monotonic release scheduler for a high-priority
control task and lower-priority telemetry task. It must make bounded release
decisions across a wrapping 32-bit tick counter without catch-up storms.

Implement the API declared by
`fixtures/rtos-periodic-scheduler/starter/periodic_scheduler.h` against the
supplied deterministic RTOS scheduler boundary.

## Target Assumptions

Target profile: `generic-rtos`. The host fixture models a single-core
preemptive scheduler where control has precedence over telemetry when both are
released. It records only release requests and absolute deadlines; no threads,
vendor task APIs, or wall-clock timing are involved. Compared release times are
always within the unsigned half-range required by the task contract.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Initialization and dispatch maintain the two declared release schedules, reject invalid state without RTOS calls, and release no task early.
- 1 point — **Bounded resource use:** The implementation uses caller-owned state and no allocation, mutable global state, spin loop, or unbounded catch-up loop.
- 2 points — **Timing behavior:** Each due release receives its fresh two- or ten-tick relative deadline, missed periods collapse deliberately, and `uint32_t` wraparound is handled with a valid half-range comparison.
- 2 points — **Concurrency safety:** Simultaneously due work is released in control-before-telemetry order, preserving the stated fixed-priority scheduling policy.
- 1 point — **Fault recovery:** A failed control or telemetry release is propagated and remains due for a later dispatch; a failed control release does not advance lower-priority work.
- 1 point — **Portability:** The answer uses portable C11 integer operations and only the supplied RTOS abstraction.
- 1 point — **Clarity and validation:** The explanation covers schedule order, deadline derivation, late-dispatch behavior, wraparound, and deterministic release/failure tests.

Do not award timing credit for rescheduling from stale releases, deadline values
derived from periods, signed-overflow time comparisons, or a loop that emits
unbounded missed releases.

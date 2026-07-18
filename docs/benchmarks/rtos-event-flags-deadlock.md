# RTOS Event Flags and Deadlock Avoidance

## Objective

Assess deterministic RTOS event consumption and safe two-mutex coordination.
The implementation must consume declared event flags with a bounded wait and
avoid an ABBA deadlock while a peer can already own the actuator mutex.

Implement the API declared by
`fixtures/rtos-event-flags-deadlock/starter/event_flags_deadlock.h` against the
supplied deterministic event-flag and mutex boundary.

## Target Assumptions

Target profile: `generic-rtos`. The fixture uses one event group and two opaque
mutexes on a single-core preemptive model. A peer may hold the actuator mutex;
contested locks return a deterministic timeout. The fixture records event wait
semantics, lock order, timeout arguments, cleanup, and apply failures without
host threads, vendor APIs, or wall-clock timing.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Initialization creates one event group and the two mutexes in the documented order, valid event signaling is exact, and wait returns the supplied RTOS result.
- 1 point — **Bounded resource use:** The implementation uses only caller-owned state and the supplied objects, with no allocation, mutable global state, spin loop, polling, or retry loop.
- 2 points — **Timing behavior:** Event waiting uses exactly two ticks and both lock attempts use exactly one tick, so no event or contention path waits indefinitely.
- 2 points — **Concurrency safety:** Wait consumes any declared event with clear-on-exit semantics, and configuration is always acquired before actuator; an unsuccessful second lock releases the first.
- 1 point — **Fault recovery:** Invalid calls make no RTOS call, creation failures leave all handles reset, apply failures still trigger both unlock attempts, and status precedence follows the contract.
- 1 point — **Portability:** The answer uses portable C11 and only the explicit opaque event/mutex interfaces.
- 1 point — **Clarity and validation:** The explanation covers event-bit consumption, global lock ordering, bounded contention, cleanup ordering, and deterministic peer-contention tests.

No concurrency credit is earned by inverted lock order, retaining consumed
events, leaking the first mutex after second-lock contention, or relying on
manual priority changes instead of the stated lock discipline.

# RTOS Priority Inversion

## Objective

Assess a deterministic repair for unbounded priority inversion around a shared
RTOS mutex. The low-priority telemetry owner must inherit high-priority safety
work while a medium-priority diagnostics task remains runnable.

Implement the API declared by
`fixtures/rtos-priority-inversion/starter/priority_inversion.h` against the
supplied deterministic RTOS boundary.

## Target Assumptions

Target profile: `generic-rtos`. The fixture uses one single-core preemptive
RTOS simulation, three statically registered tasks, and an opaque mutex API.
The low telemetry task has priority 1, diagnostics has priority 2, and safety
has priority 3. A positive-timeout contested acquisition returns
`RTOS_STATUS_BLOCKED` in the host model; callers retry after the owner unlocks.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Initialization creates and retains exactly one usable mutex, acquisition uses the declared roles, and release propagates RTOS ownership results.
- 1 point — **Bounded resource use:** The answer uses caller-owned guard state, no manual allocation, mutable global state, spin loop, or retry loop.
- 2 points — **Timing behavior:** Telemetry forwards one `RTOS_WAIT_FOREVER` acquisition, while safety forwards exactly one two-tick acquisition without extending its latency bound.
- 2 points — **Concurrency safety:** The mutex is created with priority inheritance so a blocked high-priority safety task donates priority to its low-priority telemetry owner until release.
- 1 point — **Fault recovery:** Null and uninitialized guards avoid RTOS calls, creation failure leaves a safe reset state, and RTOS lock/unlock failures are propagated.
- 1 point — **Portability:** The implementation uses only portable C11, declared fixed-width RTOS types, and the supplied opaque API.
- 1 point — **Clarity and validation:** The explanation states the scheduler assumptions and focused deterministic tests cover donation, timeout policies, initialization, and failures.

Using an ordinary mutex, semaphore, spinlock, or manual priority adjustment
does not receive concurrency-safety credit. The supplied mutex owns priority
inheritance and restoration semantics.

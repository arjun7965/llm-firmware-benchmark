# Firmware State Machine

## Objective

Assess non-blocking firmware control flow, wrap-safe timing, and recovery around
an asynchronous I2C HAL.

Implement the API declared by
`fixtures/firmware-state-machine/starter/firmware_state_machine.h` against the
supplied `fixture_hal.h` boundary.

## Target Assumptions

Target profile: `c11-mocked-hal`. See `docs/embedded/target-assumptions.md`.
The answer must still state its assumption about how HAL completion is observed.

## Scoring

Scoring profile: `firmware-v1`.

- 3 points — **Functional correctness:** Successful reads use the supplied HAL correctly and publish the required bytes, completion timestamp, validity, and latest-sample result.
- 0 points — **Bounded resource use:** The prompt sets no independent memory, allocation, stack, or buffer-capacity requirement.
- 2 points — **Timing behavior:** Poll, timeout, backoff, and cycle deadlines are wrap-safe and correctly anchored, and each step returns without waiting or spinning.
- 1 point — **Concurrency safety:** Explicit transfer ownership prevents overlapping starts and handles asynchronous completion unambiguously.
- 2 points — **Fault recovery:** After start failure, transaction failure, or timeout, retries, three-attempt exhaustion, later recovery, and sample preservation follow the required policy.
- 1 point — **Portability:** The implementation uses portable C11, the declared fixed-width types, and the supplied HAL without extensions or undefined behavior.
- 1 point — **Clarity and validation:** HAL completion assumptions are explicit and focused tests cover success, failure, timeout, exhaustion, and wraparound.

Any loop that waits synchronously for `i2c_done()` violates the central
non-blocking requirement and receives no timing-behavior credit.

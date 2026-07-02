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

- 3 points — Explicit states prevent busy-waiting and overlapping I2C transfers.
- 2 points — Poll, timeout, and backoff deadlines remain correct across timer wraparound.
- 2 points — Three-attempt retry and 10 ms backoff behavior is unambiguous.
- 2 points — Valid samples, timestamps, errors, and HAL completion assumptions are handled.
- 1 point — Tests cover success, failure, timeout, retry exhaustion, and wraparound.

Any loop that waits synchronously for `i2c_done()` violates the central
non-blocking requirement.

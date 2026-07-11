# Public Tests

The suite covers request validation, bounded-queue saturation, exact worker
concurrency, processor errors and panics, submit-versus-shutdown accounting,
graceful drain waits, deadline cancellation, listener closure, context-driven
shutdown, and SIGINT/SIGTERM propagation. `TestMain` emits a supervisor-provided
completion token only after the full suite succeeds.

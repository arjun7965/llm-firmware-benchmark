# Go Graceful Shutdown

## Objective

Assess lifecycle coordination across HTTP admission, a bounded queue, workers,
signals, and a fixed shutdown deadline.

## Scoring

- 3 points — Shutdown ordering prevents send-on-closed-channel and submit races.
- 2 points — Accepted jobs drain through four workers while new jobs receive 503.
- 2 points — HTTP shutdown, signals, cancellation, and the 10-second deadline compose correctly.
- 2 points — Queue-full behavior, validation, handler errors, and worker errors are defined.
- 1 point — Runnable tests cover submit-versus-shutdown and deadline behavior.

Closing a channel while handlers may still send to it cannot receive full race
safety credit.

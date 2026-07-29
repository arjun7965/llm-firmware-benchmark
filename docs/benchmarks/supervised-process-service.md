# Supervised Process Service

## Objective

Assess a bounded embedded-Linux supervisor that communicates with one
privilege-separated worker, detects worker exit without PID polling races,
retries unacknowledged work under a capped policy, and shuts down without
leaking a child or descriptor.

Implement the API declared by
`fixtures/supervised-process-service/starter/supervised_service.h` using the
supplied process-launch boundary and the stated Linux/POSIX interfaces.

## Target Assumptions

Target profile: `embedded-linux-posix`. Validation uses little-endian Ubuntu
24.04 x86-64 LP64 and GCC 13.3.0. One supervisor invocation runs on the main
thread. A supplied launcher owns safe fork/exec setup and returns one pidfd and
one nonblocking `SOCK_SEQPACKET` channel. The fixture redirects process, IPC,
polling, signal, termination, and reaping calls, so tests create no real child.
There is no heap, thread, shared memory, filesystem durability, MMIO, DMA,
cache, or hard real-time deadline.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Valid inputs launch the declared worker, encode exact bounded request records, accept only a matching six-byte acknowledgement, advance each accepted message once, and return the documented rejection/protocol results.
- 1 point — **Bounded resource use:** The implementation owns one child, one pidfd, one sequence-preserving channel, one self-pipe, one in-flight message, and fixed stack buffers, with no heap, threads, extra processes, or unbounded queue.
- 2 points — **Timing behavior:** Send/ack waits, SIGTERM grace, forced-kill completion, and restart waits use the exact bounded poll timeouts; restart backoff is 100, 200, 400, 400... ms and resets only after acceptance.
- 1 point — **Concurrency safety:** SIGINT/SIGTERM publication is async-signal-safe, preserves `errno`, uses only `sig_atomic_t` shared state plus one best-effort write, and stop wins over simultaneous worker readiness.
- 2 points — **Fault recovery:** Exit/channel/timeout failures reap the prior child, resend the same unacknowledged record, enforce three consecutive restarts, distinguish fatal spawn/syscall failures, and use bounded SIGTERM-to-SIGKILL cleanup on every terminal path.
- 1 point — **Portability:** Wire integers use explicit little-endian encoding, IPC relies only on the stated Linux/POSIX contract, and launch-specific fork/exec hygiene remains behind the supplied adapter.
- 1 point — **Clarity and validation:** The explanation covers at-least-once delivery, pidfd ownership, restart/reset policy, signal safety, shutdown escalation, cleanup, and deterministic lifecycle tests.

Do not award recovery credit to an implementation that drops an unacknowledged
message, leaves a zombie, restarts forever, or resets its budget merely because
spawn succeeded. Do not award timing credit to sleep-based, busy-waiting, or
unbounded shutdown/restart logic.

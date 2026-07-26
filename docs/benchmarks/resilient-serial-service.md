# Resilient Serial Service

## Objective

Assess a bounded embedded-Linux serial ingestion service that configures a
device safely, remains responsive to process shutdown, and reconnects after
expected hot-unplug and availability failures without spinning.

Implement the API declared by
`fixtures/resilient-serial-service/starter/serial_service.h` using the stated
C11, GNU/Linux, and POSIX interfaces.

## Target Assumptions

Target profile: `embedded-linux-posix`. Validation uses little-endian Ubuntu
24.04 x86-64 LP64 and GCC 13.3.0, while the implementation contract remains
portable to little-endian embedded Linux with `pipe2` and POSIX.1-2008 device,
polling, signal, and termios behavior. Exactly one invocation runs on the main
thread. The fixture replaces POSIX calls deterministically; it does not require
a physical serial device. There is no heap, child process, MMIO, DMA, cache,
filesystem durability, or hard real-time deadline.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Valid startup opens the declared path with the exact flags, establishes raw 115200-8N1 termios state, and forwards each positive read once and unchanged through the callback.
- 1 point — **Bounded resource use:** The implementation uses one 64-byte stack buffer, one serial descriptor, one two-descriptor self-pipe, and at most one read per readiness event, with no heap, threads, child processes, or unbounded buffering.
- 2 points — **Timing behavior:** The first open is immediate; reconnect waits use `poll` rather than sleeping or spinning; delays progress through 100, 200, 400, 800, and capped 1600 ms values; and successful configuration resets the delay.
- 1 point — **Concurrency safety:** SIGINT/SIGTERM publication uses only `volatile sig_atomic_t` state and one best-effort async-signal-safe write while preserving `errno`; stop has priority over simultaneous device input.
- 2 points — **Fault recovery:** Expected open/configuration/read loss, EOF, and terminal poll events close and reconnect; transient errors retain the connection; permanent errors and consumer rejection return the documented result; every terminal path restores handlers and closes acquired descriptors.
- 1 point — **Portability:** The answer follows C11 and the stated GNU/Linux/POSIX contract, configures termios without device-specific ioctls, and uses no undocumented hardware, vendor library, or filesystem assumption.
- 1 point — **Clarity and validation:** The explanation covers signal safety, termios, error classification, reconnect/reset behavior, cleanup, and focused deterministic lifecycle tests.

Do not award signal-safety credit to a handler that calls non-async-signal-safe
interfaces, and do not award timing credit to sleep-based, busy-waiting,
uncapped, or non-resetting reconnect logic.

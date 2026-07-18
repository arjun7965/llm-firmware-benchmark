# Starter Interface

`deferred_work.h` defines the caller-owned dispatcher and its foreground and
ISR entry points. `fixture_interrupt_work.h` exposes only opaque volatile
latch and interrupt-mask accessors. The implementation must not inspect or
cast the latch, and volatile latch access does not replace C11 atomic
synchronization for the shared deferred-work bitset.

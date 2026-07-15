# Deterministic RTOS Mock

The mock exposes one mutex and three registered tasks. Larger numeric priorities
run first. A positive-timeout contested lock returns `RTOS_STATUS_BLOCKED`,
marks the caller blocked, and donates that waiter's effective priority to the
owner only when the mutex was created with priority inheritance.

Unlocking makes blocked waiters runnable; the test explicitly retries their
lock operation. This models the observable scheduler state without threads,
wall-clock timing, vendor APIs, or physical hardware.

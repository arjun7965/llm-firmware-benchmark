# RTOS Event Flags and Deadlock Starter API

`event_flags_deadlock.h` defines a caller-owned supervisor coordination object.
It accepts three event bits, waits for any event with a two-tick bound, and
applies configuration under two mutexes with one global lock order.

`fixture_rtos_events.h` is the complete opaque RTOS boundary. The answer must
not use mock-only headers, vendor APIs, dynamic allocation, spin loops, or
manual task-priority changes.

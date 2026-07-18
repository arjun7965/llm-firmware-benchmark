# RTOS Periodic Scheduler Starter API

`periodic_scheduler.h` is the complete answer boundary. It owns only the next
release ticks for a high-priority control task and a lower-priority telemetry
task.

`fixture_rtos_scheduler.h` is the only RTOS API available to the answer. It
records one release request at a time; model answers must not include mock-only
headers or use vendor scheduler APIs.

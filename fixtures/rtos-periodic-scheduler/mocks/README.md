# Deterministic RTOS Scheduler Mock

The mock records release requests and their absolute deadlines. It can inject
one release failure, allowing public tests to verify that a due task is not
silently skipped. It contains no threads, wall clock, or vendor scheduler.

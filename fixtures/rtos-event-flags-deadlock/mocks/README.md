# Deterministic RTOS Events Mock

The mock owns one event group and two mutexes. It exposes event-bit clearing,
bounded waits, lock order, an externally held actuator mutex, and injected
lock/unlock/apply failures without creating threads or a real RTOS.

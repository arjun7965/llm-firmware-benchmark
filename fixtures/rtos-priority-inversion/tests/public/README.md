# Public Tests

The host tests validate initialization recovery, exact timeout forwarding,
error propagation, and the priority-inheritance schedule: after high-priority
safety blocks on the low-priority telemetry owner, telemetry must outrank the
runnable medium-priority diagnostics task until it releases the mutex.

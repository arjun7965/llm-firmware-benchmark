# Public Tests

The public C tests verify the exact compiler-diagnostic and execution-trace
correlation, stale-output replacement, initialization boundaries, the captured
320-byte regression, chunk sizes around 8-bit and 64-byte boundaries, terminal
idempotence, and rejection of malformed caller-owned state without mutation.

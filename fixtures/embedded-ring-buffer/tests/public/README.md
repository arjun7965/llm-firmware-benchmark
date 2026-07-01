# Public Tests

`test_ring_buffer.c` covers invalid initialization, FIFO ordering, use of every
declared slot, drop-new overflow, capacity one, repeated slot reuse, and
unsigned atomic-counter wraparound.

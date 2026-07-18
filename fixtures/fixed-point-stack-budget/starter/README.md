# Fixed-Point Stack-Budget Starter API

Implement `fixed_point_stack.h` in one C11 source file. The supplied stack is
a simulated downward-growing thread stack: bytes from its low address through
the first non-fill byte are still unused. Do not alter that caller-owned stack
region.

The task uses Q8.8 samples, a nonnegative Q1.15 gain, a Q8.8 offset, signed
half-away-from-zero rounding, and saturating `int16_t` output. The C11 compile
contract rejects variable-length arrays.

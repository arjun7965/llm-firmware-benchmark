# Fixed-Point Stack Budget

## Objective

Assess Q-format arithmetic under an explicit simulated stack high-water budget, with bounded batch work, stack alignment, signed tie rounding, and saturating output.

Implement the API declared by `fixtures/fixed-point-stack-budget/starter/fixed_point_stack.h`.

## Target Assumptions

Target profile: `portable-c11`. The caller owns an eight-byte-aligned simulated downward-growing stack filled with `0xA5`; the untouched low-address prefix is its available stack space. The host compiler rejects variable-length arrays, and there is no heap, floating-point requirement, thread, RTOS, or hardware dependency.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** The worker validates and records its stack contract, calculates the high-water usage, applies Q1.15 gain and Q8.8 offset, supports in-place batches, and saturates int16_t output.
- 2 points — **Bounded resource use:** Processing is capped at eight samples and uses scalar fixed-width intermediates with no heap, VLA, recursion, mutable global state, or writes to caller-owned stack memory.
- 1 point — **Timing behavior:** Each accepted batch has a fixed maximum loop bound and performs the stack-budget gate before changing any output.
- 1 point — **Concurrency safety:** Under the documented caller-serialized worker contract, validation and output publication keep the worker configuration and each accepted sample result coherent.
- 2 points — **Fault recovery:** Invalid initialization, invalid worker/pointers, oversized batches, and over-budget watermarks fail without changing output; zero-length batches have the declared no-access success behavior.
- 1 point — **Portability:** The implementation avoids floating point and implementation-defined signed shifts, uses int64_t intermediates, explicit signed half-away-from-zero division, and defined saturation.
- 1 point — **Clarity and validation:** The explanation covers the Q formats, negative tie behavior, saturation, stack fill scan, exact budget, and deterministic test cases.

Truncating negative ties, wrapping rather than saturating, measuring untouched bytes as used stack, accepting a misaligned stack, or processing an over-budget worker cannot receive the relevant correctness, portability, or recovery credit.

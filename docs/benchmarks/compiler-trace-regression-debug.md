# Compiler-Diagnostic and Execution-Trace Regression Rubric

## Objective

Correlate a compiler narrowing diagnostic with the first divergent event in a
captured DMA-planner execution trace, then implement the exact full-width
arithmetic repair. The answer must return one C implementation of the supplied
diagnosis and chunk-planning API without inventing DMA hardware behavior.

## Target Assumptions

Target profile: `armv7m-bare-metal`. The defective firmware was built for a
single-core little-endian Cortex-M3 using ARMv7-M and AAPCS/EABI. `uint8_t` is
8 bits and `uint32_t` is 32 bits. Validation runs the portable diagnosis and
planner API on the host; it neither submits descriptors nor accesses hardware.

## Scoring

Scoring profile: `firmware-v1`.

- 3 points — **Functional correctness:** Connects the line-16 `uint32_t` to
  `uint8_t` warning to 256 truncating modulo 256 to zero at offset 64, reports
  every captured diagnosis value, and plans complete transfers in one-to-64
  byte chunks with exact offsets and final-chunk flags.
- 1 point — **Bounded resource use:** Each API performs fixed bounded work with
  no allocation, hardware access, polling, retry, or loop.
- 1 point — **Timing behavior:** Every planner call produces at most one chunk
  and has no hardware-dependent wait or scheduling assumption.
- 1 point — **Concurrency safety:** Uses caller-owned state and no mutable
  global state, and does not falsely claim concurrent access is safe.
- 2 points — **Fault recovery:** Rejects null, out-of-range, uninitialized, and
  internally inconsistent state without changing the state or output; valid
  initialization replaces stale state; completion is stable and idempotent.
- 1 point — **Portability:** Keeps the subtraction and chunk arithmetic in
  `uint32_t`, respects the documented 32-bit target widths, and does not rely
  on host `int`, `long`, or pointer widths.
- 1 point — **Clarity and validation:** Concisely explains the diagnostic,
  modulo truncation, trace divergence, invariant checks, repair, and focused
  boundary tests.

Partial credit is appropriate when the narrowing cause is identified but the
trace correlation, state invariants, transfer boundaries, or no-mutation
guarantees are incomplete.

## Validation

The deterministic C11 fixture compiles the submitted module with public tests.
Tests check exact diagnosis replacement, null handling, valid and invalid
initialization, the captured 320-byte trace, totals around 64- and 256-byte
boundaries, stable completion, and malformed state rejection. Mutation
calibration covers each diagnosis conclusion and planner guarantee.

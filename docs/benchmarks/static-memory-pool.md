# Static Memory Pool

## Objective

Assess deterministic static allocation with embedded fixed-size storage, alignment guarantees, exhaustive allocation, exact release validation, and reinitialization.

Implement the API declared by `fixtures/static-memory-pool/starter/static_memory_pool.h`.

## Target Assumptions

Target profile: `portable-c11`. The caller owns a four-block, 32-byte-per-block pool object whose storage begins on a 16-byte boundary. Calls are serialized by the caller; the fixture evaluates the allocator's state transition, not a thread-safe heap implementation.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Initialization, lowest-free allocation, exact capacity, availability accounting, reuse, and reinitialization behave as declared.
- 2 points — **Bounded resource use:** Storage and metadata remain embedded in the caller-owned pool, with no heap API, VLA, recursion, or mutable global allocator state.
- 1 point — **Timing behavior:** Allocation, release, and availability use only fixed four-entry bounded work and do not wait, retry, or scan an unbounded structure.
- 1 point — **Concurrency safety:** The implementation preserves a coherent allocation map under the documented caller-serialized contract and never exposes an unmarked returned block.
- 2 points — **Fault recovery:** Null, uninitialized, exhausted, outside, interior, and double-release inputs leave allocation state unchanged and return the required failure result.
- 1 point — **Portability:** The implementation uses portable C11 alignment and integer-address offset checks without relational comparisons between unrelated pointers.
- 1 point — **Clarity and validation:** The explanation covers static ownership, alignment, deterministic exhaustion/reuse, invalid-release isolation, and representative tests.

Allocating the same block twice, accepting an interior pointer, dynamically allocating backing storage, or retaining allocations after reinitialization cannot receive the corresponding correctness, resource, or recovery credit.

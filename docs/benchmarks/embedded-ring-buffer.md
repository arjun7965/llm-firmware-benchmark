# Embedded Ring Buffer

## Objective

Assess portable lock-free reasoning for a C11 single-producer/single-consumer
queue shared between an interrupt and a main loop.

Implement the three functions declared by
`fixtures/embedded-ring-buffer/starter/ring_buffer.h`. Storage is supplied by
the caller. A full queue rejects the new byte and leaves existing bytes intact.

## Target Assumptions

Target profile: `c11-lock-free-spsc`. See
`docs/embedded/target-assumptions.md`.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Valid initialization, use of every declared capacity slot, FIFO ordering, and successful push/pop results are correct.
- 1 point — **Bounded resource use:** The implementation uses caller-owned storage and performs no dynamic allocation.
- 1 point — **Timing behavior:** Push and pop use a lock-free atomic index type and return without waiting, busy-waiting, or a blocking primitive.
- 3 points — **Concurrency safety:** Producer and consumer ownership plus C11 acquire/release ordering prevent data races and publish slot contents correctly.
- 1 point — **Fault recovery:** Zero or non-power-of-two capacity is rejected, a full push leaves existing bytes intact, and an empty pop returns false.
- 1 point — **Portability:** Unsigned counter wraparound and index masking avoid undefined behavior and unstated target-width assumptions.
- 1 point — **Clarity and validation:** Assumptions are reviewable and deterministic tests cover empty, full, ordering, and counter wraparound.

Using `volatile` alone for inter-context synchronization receives no
concurrency-safety credit.

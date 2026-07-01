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

- 3 points — Atomic ownership and acquire/release ordering correctly synchronize data.
- 2 points — Index arithmetic preserves the full power-of-two capacity without ambiguity.
- 2 points — Push and pop are non-blocking and implement the required drop-new behavior.
- 2 points — The C11 implementation avoids data races, overflow mistakes, and undefined behavior.
- 1 point — Deterministic tests exercise empty, full, wraparound, and ordering cases.

Using `volatile` alone for inter-context synchronization receives no atomic
correctness credit.

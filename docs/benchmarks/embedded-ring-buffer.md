# Embedded Ring Buffer

## Objective

Assess portable lock-free reasoning for a C11 single-producer/single-consumer
queue shared between an interrupt and a main loop.

## Scoring

- 3 points — Atomic ownership and acquire/release ordering correctly synchronize data.
- 2 points — Index arithmetic preserves the full power-of-two capacity without ambiguity.
- 2 points — Push and pop are non-blocking with explicit, safe overflow behavior.
- 2 points — The C11 implementation avoids data races, overflow mistakes, and undefined behavior.
- 1 point — Deterministic tests exercise empty, full, wraparound, and ordering cases.

Using `volatile` alone for inter-context synchronization receives no atomic
correctness credit.

# RTOS Queue and Semaphore

## Objective

Assess a deterministic producer/worker handoff that pairs a fixed-capacity FIFO
queue with one counting-semaphore token per accepted sensor sample.

Implement the API declared by
`fixtures/rtos-queue-semaphore/starter/queue_semaphore.h` against the supplied
deterministic queue and semaphore boundary.

## Target Assumptions

Target profile: `generic-rtos`. The fixture owns one four-item FIFO and one
matching counting semaphore. It records each call's order and timeout argument,
preserves sample order, and models every API call as an immediate deterministic
status rather than a host thread or a vendor RTOS implementation.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Initialization creates the declared queue and zero-token semaphore once, publishing and taking preserve FIFO samples, and invalid state has no RTOS side effect.
- 1 point — **Bounded resource use:** The answer uses only the caller-owned pipeline and supplied fixed-capacity objects, with no allocation, polling, spin loop, mutable global state, or retry loop.
- 2 points — **Timing behavior:** Queue send and receive use immediate timeouts, while worker acquisition uses exactly the declared three-tick bound rather than an indefinite wait.
- 2 points — **Concurrency safety:** A successful enqueue occurs before its semaphore give, a worker takes before it receives, and full-queue rejection cannot create a false token.
- 1 point — **Fault recovery:** Create, send, give, take, and receive failures are propagated at their documented boundary without hidden compensation or an extra RTOS call.
- 1 point — **Portability:** The implementation uses portable C11, the declared fixed-width sample type, and the supplied opaque queue/semaphore API only.
- 1 point — **Clarity and validation:** The explanation states the queue/token invariant, capacity and ordering behavior, bounded worker wait, and deterministic failure tests.

A binary semaphore, give-before-send ordering, receive-before-take ordering, or
an infinite worker wait cannot receive the concurrency or timing points.

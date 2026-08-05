# Mixed C/C++ MMIO Safety Review

## Objective

Assess a C++17 review and repair of defective mixed-language MMIO ownership
code: source-level language hazards, checked conversion, opaque C ABI use, and
move-only RAII cleanup.

Implement `fixtures/mixed-c-cpp-mmio-safety-review/starter/mmio_safety_review.hpp`.

## Target Assumptions

Target profile: `armv7m-bare-metal`. The fictional peripheral is a single-core,
little-endian Cortex-M3/AAPCS target. The fixture supplies the complete volatile
C accessor ABI; no direct register layout, pointer width, `long`, aliasing,
DMA, cache, interrupt, heap, or vendor SDK assumption is permitted. Hosted
GCC C11/C++17 validates the accessor ordering only.

The caller must ensure that at most one live, active `mmio_transfer_t` owns a
given MMIO handle. The move assignment of two different active owners therefore
requires distinct peripheral handles; self-move is safe. This object has no
global registry and cannot enforce cross-object handle exclusivity.

## Scoring

Scoring profile: `firmware-v1`.

- 3 points — **Functional correctness:** Required fixed-line findings and
  start/poll results are correct.
- 1 point — **Bounded resource use:** No heap, containers, exceptions, RTTI,
  mutable global state, retry loop, or unbounded work is introduced.
- 1 point — **Timing behavior:** Operations are immediate and non-blocking;
  `poll` takes one status snapshot and never spins.
- 1 point — **Concurrency safety:** A transfer object prevents overlap in its
  own operations and publishes ownership only after peripheral setup; the
  caller-owned per-handle exclusivity precondition is respected.
- 2 points — **Fault recovery:** Invalid input, terminal status,
  cancellation, destruction, moves, and self-move preserve exact cleanup.
- 1 point — **Portability:** The opaque C ABI, checked conversion, and no
  aliasing, layout, or host-width assumptions are respected.
- 1 point — **Clarity and validation:** The explanation distinguishes
  MISRA-style concerns from formal compliance and describes focused tests.

Missing a required diagnosis, accepting a count above 4095, copying an MMIO
owner, direct register access, or leaving an active transfer enabled cannot
receive the corresponding functional, ownership, recovery, or portability
credit.

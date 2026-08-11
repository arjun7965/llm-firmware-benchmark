# Fixed-Point Filter Optimization

## Objective

Assess fixed-point FIR optimization under explicit numerical-error and modeled
cycle budgets, including exact symmetry reduction, signed rounding, saturation,
history ownership, and transactional cost failure.

Implement the API declared by
`fixtures/fixed-point-filter-optimization/starter/fixed_point_filter.h`.

## Target Assumptions

Target profile: `portable-c11`. Inputs, coefficients, history, and outputs use
Q1.15. A fixture-owned opaque cost model assigns twelve fixed cycles and four
cycles per MAC. This deterministic model is an architecture-independent
benchmark contract, not a claim about wall-clock timing on a specific CPU.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Initialization establishes zero
  history; each accepted step implements the declared seven-tap response,
  signed nearest rounding, saturation, and correct history advancement.
- 2 points — **Bounded resource use:** Exact symmetry reduces seven tap
  products to four accessor-instrumented MACs with fixed-size state, no heap,
  VLA, recursion, floating point, or mutable global state.
- 1 point — **Timing behavior:** Every accepted sample declares four MACs and
  consumes exactly 28 modeled cycles; a 27-cycle capacity fails before work.
- 1 point — **Concurrency safety:** Under the documented caller-serialized
  contract, output and history are published only after the cost transaction
  commits, so a failed step leaves a coherent prior state.
- 2 points — **Fault recovery:** Invalid arguments, insufficient capacity, and
  commit failure return false without changing filter state or output; a later
  valid step remains usable.
- 1 point — **Portability:** Pair sums and accumulation use int32_t and int64_t,
  signed division avoids implementation-defined shifts, and saturation prevents
  narrowing overflow.
- 1 point — **Clarity and validation:** The explanation relates the full and
  symmetric convolutions, the one-LSB error certificate, cycle accounting,
  impulse/tie/saturation cases, and transactional tests.

Using seven MACs, rounding each tap independently, truncating negative ties,
wrapping overshoot, shifting history after a failed commit, or claiming a
hardware cycle count beyond the supplied model cannot receive the relevant
correctness, timing, recovery, or portability credit.

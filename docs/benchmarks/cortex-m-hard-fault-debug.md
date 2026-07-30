# Cortex-M Hard-Fault Debugging Rubric

## Objective

Diagnose a forced Cortex-M3 HardFault from the supplied exception frame, SCB
register snapshot, linker map, disassembly, and defective source, then implement
the exact one-past boundary repair. The answer must return one C implementation
of the supplied API and must not invent live hardware access.

## Target Assumptions

Target profile: `armv7m-bare-metal`. The evidence comes from a single-core
little-endian Cortex-M3 using ARMv7-M and AAPCS/EABI, with a basic exception
frame and no FPU. Validation runs the portable diagnosis and repair API on the
host; it does not read target registers or execute target instructions.

## Scoring

Scoring profile: `firmware-v1`.

- 3 points — **Functional correctness:** Decodes the precise BusFault and
  forced escalation, selects and reports the PSP basic frame, preserves the raw
  stacked LR, symbolizes the precise PC and normalized caller return address,
  correlates the indexed word store with BFAR and the half-open `crash_log`
  range, and implements the exact `index >= HARD_FAULT_LOG_WORDS` repair.
- 1 point — **Bounded resource use:** Both APIs have fixed, bounded work with no
  polling, retry, allocation, or unbounded loop.
- 1 point — **Timing behavior:** Diagnosis and repair are constant-time and the
  response does not introduce hardware-dependent waits or timing assumptions.
- 1 point — **Concurrency safety:** Uses no mutable global state and does not
  falsely claim that the plain log write is safe for concurrent callers.
- 2 points — **Fault recovery:** Null inputs and every out-of-range index fail
  without mutation, valid boundary indices write exactly one requested word,
  and every diagnosis field is replaced so stale output cannot survive.
- 1 point — **Portability:** Uses the supplied fixed-width types, treats mapped
  object ranges as half-open, and clears the Thumb bit only in the address used
  to symbolize the caller.
- 1 point — **Clarity and validation:** Concisely connects the register bits,
  frame selection, symbols, disassembly, effective-address calculation, source
  defect, repair, and deterministic test strategy.

Partial credit is appropriate when the main fault is identified but one
cross-evidence conclusion, boundary guarantee, or API edge case is wrong.

## Validation

The deterministic C11 fixture compiles the submitted module with public tests.
Tests check every diagnosis field, stale-output replacement, null behavior,
exact valid writes at indices zero and seven, rejection of index eight and
`SIZE_MAX`, and preservation of adjacent words and canaries. Mutation
calibration covers each evidence conclusion and repair guarantee.

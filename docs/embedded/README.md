# Embedded and Firmware Benchmark Plan

This directory defines how embedded coverage is selected and how target
assumptions are made comparable across model runs.

- `capability-matrix.md` records current coverage and planned task families.
- `target-assumptions.md` defines reusable execution profiles and the minimum
  assumptions every embedded task must state.
- `../benchmarks/firmware-scoring.md` defines the common scoring dimensions
  used by embedded and firmware task rubrics.
- `../../fixtures/` contains the validated per-task scaffold and manifest
  layout used by future executable checks.
- `../../scripts/check-cross-compilation.mjs` optionally compiles trusted C
  references for representative ARMv7-M and RV32 targets.

The matrix is a planning artifact, not a claim that listed hardware or
toolchains are installed. A capability is considered covered only after its
task, rubric, fixtures, and validation path are committed and calibrated.
Cross-compilation is a portability probe; it does not activate a planned target
profile or replace task-specific execution tests.

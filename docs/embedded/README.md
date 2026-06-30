# Embedded and Firmware Benchmark Plan

This directory defines how embedded coverage is selected and how target
assumptions are made comparable across model runs.

- `capability-matrix.md` records current coverage and planned task families.
- `target-assumptions.md` defines reusable execution profiles and the minimum
  assumptions every embedded task must state.
- `../../fixtures/` contains the validated per-task scaffold and manifest
  layout used by future executable checks.

The matrix is a planning artifact, not a claim that listed hardware or
toolchains are installed. A capability is considered covered only after its
task, rubric, fixtures, and validation path are committed and calibrated.

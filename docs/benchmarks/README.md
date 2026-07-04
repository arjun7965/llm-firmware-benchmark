# Benchmark Rubrics

Each file in this directory explains the intent and scoring criteria for the
matching task ID in `tasks.json`. Score an answer from 0 to 10 using the listed
point allocations. Award partial credit within a criterion when the approach is
sound but incomplete.

Evaluate the submitted answer as written. Do not infer missing code or silently
repair it. Apply explicit prompt constraints, including language, dependency,
word-limit, and testing requirements. A response that violates a safety-critical
requirement cannot receive the points assigned to that requirement.

For comparable results:

1. Blind model identities during initial scoring.
2. Use the same rubric version for every compared run.
3. Record concise reasons for deductions.
4. Resolve ambiguous rubric interpretations before unblinding.
5. Disclose any automated compilation, testing, or static-analysis checks.

The rubric files are human-readable policy. `schemas/tasks.schema.json` and
`schemas/repeat-scores.schema.json` define the machine-readable input formats.
See `docs/dependencies.md` for optional compilation and execution toolchains.
Tasks with a `targetProfile` also follow the versioned dimensions and scoring
procedure in `firmware-scoring.md`.

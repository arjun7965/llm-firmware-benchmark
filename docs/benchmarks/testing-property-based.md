# Property-Based Path Testing

## Objective

Assess whether the model can design an executable property-based test suite
with an independent oracle and useful generators.

## Scoring

- 3 points — The reference oracle directly and correctly implements the stated contract.
- 2 points — Generators cover Unicode, separators, dot segments, and shrinking-friendly structure.
- 2 points — Properties cover absoluteness, idempotence, root confinement, and preservation.
- 2 points — Targeted cases cover NUL rejection and difficult normalization boundaries.
- 1 point — The pytest and Hypothesis module is executable and imports the system under test.

Reusing `normalize_path` inside the oracle defeats independence and loses the
oracle criterion.

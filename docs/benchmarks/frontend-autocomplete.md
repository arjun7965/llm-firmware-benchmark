# Frontend Autocomplete

## Objective

Assess whether the model can coordinate asynchronous React behavior,
accessibility, and interaction state without external packages.

## Scoring

- 3 points — Debouncing, cancellation, and stale-response suppression are race-safe.
- 2 points — Keyboard behavior and combobox/listbox ARIA relationships are correct.
- 2 points — Controlled input plus loading, empty, error, and selection states work.
- 2 points — React and TypeScript code is self-contained, maintainable, and leak-free.
- 1 point — The explanation identifies the important races and test cases.

Do not award full asynchronous-correctness credit if an older request can
replace newer results, even when `AbortController` is present.

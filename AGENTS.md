# Repository Guidelines

## Project Structure & Module Organization

- `tasks.json` is the source of truth for task IDs, categories, and prompts.
- `models.example.json` documents the ignored local model configuration.
- `src/` contains provider-neutral orchestration, provider adapters, model configuration, and statistics.
- `test/` contains `node:test` coverage; adapter tests such as `ncode.test.mjs` stay separate from generic harness tests.
- `run-benchmark.mjs` runs every task against the configured local models and writes first-run records to `results/`.
- `run-repeats.mjs` creates additional samples under `results/run-2/` and `results/run-3/`.
- `summarize-repeats.mjs` calculates variance statistics from `repeat-scores.json`.
- `results/` contains ignored local outputs; generated answers and reports are
  not repository content.

Generated files follow `<task-id>--<model-name>.json`; keep task IDs lowercase and hyphen-separated.

## Build, Test, and Development Commands

There is no build step. Use Node.js 22+, copy `models.example.json` to
`models.local.json`, and install the runtime required by each provider.

```bash
node run-benchmark.mjs       # Run the primary task/model matrix
node run-repeats.mjs         # Generate runs 2 and 3
node summarize-repeats.mjs   # Print statistics from manual scores
node --test                  # Run harness unit tests without models
node --check run-benchmark.mjs
```

Runs may take five minutes per job. Successful results are skipped. Preserve raw
JSON privately; it is ignored because generated output can contain sensitive data.

## Coding Style & Naming Conventions

Follow the existing JavaScript style: ESM imports, two-space indentation, semicolons, double quotes, trailing commas in multiline structures, and `camelCase` identifiers. Use `const` by default and small focused functions. Keep task and model mappings explicit so result filenames remain stable. Format JSON with two-space indentation.

## Testing Guidelines

Tests use Node's built-in `node:test`; name files `*.test.mjs`. Run `npm test` and `npm run check`. Run `npm run summarize` after aggregation changes. Validate edited JSON before review. For runner changes, use a constrained smoke test or explain why a model run was omitted.

## Commit & Pull Request Guidelines

No usable Git history is present in this checkout, so no repository-specific commit convention can be inferred. Use short, imperative subjects such as `Add retry metadata to benchmark results`. Pull requests should describe the benchmark behavior changed, list validation performed, identify generated artifacts, and disclose changes that affect model parity, scoring, concurrency, timeouts, or harness conditions. Do not mix regenerated result data with unrelated code edits.

## Evaluation Integrity

Keep prompts identical across compared models. Preserve raw outputs outside Git,
publish only sanitized artifacts, and disclose provider or harness differences.

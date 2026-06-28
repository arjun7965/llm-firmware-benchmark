# Repository Guidelines

## Project Structure & Module Organization

- `tasks.json` defines task IDs, categories, and prompts;
  `models.example.json` documents local model configuration.
- `src/` contains orchestration, statistics, configuration, and providers.
- `test/` contains `node:test` coverage; keep provider tests separate from
  generic harness tests.
- `docs/benchmarks/` contains task rubrics; `schemas/` defines JSON contracts.
- Top-level runner scripts generate and summarize ignored records in `results/`.

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
npm run security:scan        # Detect credentials and personal paths
```

Use `npm run benchmark -- --help` for model/task filters, run selection,
concurrency, alternate input files, and output location.

Runs may take five minutes per job. Successful results are skipped. Preserve raw
JSON privately; it is ignored because generated output can contain sensitive data.

## Coding Style & Naming Conventions

Use ESM imports, two-space indentation, semicolons, double quotes, multiline
trailing commas, and `camelCase`. Prefer `const` and focused functions. Keep
task/model mappings explicit and format JSON with two-space indentation.

## Testing Guidelines

Tests use Node's built-in `node:test`; name files `*.test.mjs`. Provider tests
must mock CLI or HTTP boundaries and not require credentials or live
models. Run `npm test`, `npm run check`, and `npm run security:scan`. Run
`npm run summarize` after aggregation changes. Every task needs a matching
ten-point rubric under `docs/benchmarks/`. For runner changes, use a constrained
smoke test or explain why a model run was omitted.

## Commit & Pull Request Guidelines

Use short, imperative subjects such as `Add retry metadata to benchmark
results`. Pull requests should describe behavior changes and validation, and
disclose changes affecting model parity, scoring, concurrency, or timeouts.

## Evaluation Integrity

Keep prompts identical across compared models. Preserve raw outputs outside Git,
publish only sanitized artifacts, and disclose provider or harness differences.
Never publish `results/` directly. Use `npm run export:public`, inspect every
file marked `reviewRequired`, and follow `docs/publishing-results.md`.

# Repository Guidelines

## Project Structure & Module Organization

- `tasks.json` defines task IDs, categories, and prompts;
  `models.example.json` documents model configuration.
- `src/` contains harness modules and providers.
- `test/` contains `node:test` coverage, including isolated provider tests.
- `docs/` contains rubrics, dependencies, and embedded plans; `schemas/`
  defines JSON contracts.
- `fixtures/<task-id>/` contains manifests and task assets.
- Top-level scripts manage ignored records in `results/`.

Outputs follow `<task-id>--<model-name>.json`; task IDs use lowercase
hyphenated words. Embedded and firmware tasks require `targetProfile`.

## Build, Test, and Development Commands

Use Node.js 22+ and copy `models.example.json` to `models.local.json`.

```bash
node run-benchmark.mjs       # Run task/model matrix
node run-repeats.mjs         # Generate runs 2 and 3
node summarize-repeats.mjs   # Print statistics from manual scores
npm test                     # Run tests without models
npm run check                # Syntax-check JavaScript
npm run security:scan        # Detect credentials and personal paths
npm run fixtures:check       # Validate fixture manifests and paths
npm run fixture:extract -- --result <result.json>
npm run fixture:validate -- --task <task-id>
npm run test:c               # Run C fixtures and mutants
npm run cross:check          # Probe optional cross-compilers
```

Use `npm run benchmark -- --help` for model/task filters, run selection,
concurrency, alternate input files, and output location.

## Coding Style & Naming Conventions

Use ESM imports, two-space indentation, semicolons, double quotes, multiline
trailing commas, and `camelCase`. Prefer `const` and focused functions. Keep
task/model mappings explicit and format JSON with two-space indentation.

## Testing Guidelines

Tests use `node:test`; name files `*.test.mjs`. Provider tests must mock CLI or
HTTP boundaries without credentials or live models. Run `npm test`,
`npm run check`, and `npm run security:scan`. Run
`npm run summarize` after aggregation changes. Every task needs a matching
ten-point rubric under `docs/benchmarks/`. For runner changes, use a constrained
smoke test or explain why a model run was omitted. Document any new task
toolchain in `docs/dependencies.md`; embedded profiles must be defined in
`src/target-profiles.mjs` and documented under `docs/embedded/`. Keep fixture
commands as argv arrays; never invoke model output through a shell.

## Commit & Pull Request Guidelines

Use short, imperative subjects such as `Add retry metadata to benchmark
results`. Pull requests should describe behavior changes and validation, and
disclose changes affecting model parity, scoring, concurrency, or timeouts.

## Evaluation Integrity

Keep prompts identical across compared models. Preserve raw outputs outside Git,
publish only sanitized artifacts, and disclose provider or harness differences.
Never publish `results/` directly. Use `npm run export:public`, inspect every
file marked `reviewRequired`, and follow `docs/publishing-results.md`.

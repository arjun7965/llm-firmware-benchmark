# NCODE.md

This file provides guidance to NCode when working with code in this repository.

## Repository purpose

This is a provider-oriented benchmark harness that runs the same coding prompts
against any locally configured set of LLMs and stores responses for private
evaluation. The included provider adapter invokes the `ncode` CLI.

## Running the benchmark

Scripts use Node.js ESM (.mjs) and have no package dependencies.

- Copy `models.example.json` to the ignored `models.local.json`, then configure
  model IDs, providers, and provider-specific options.

- Run the main benchmark (all configured tasks × models):

  ```bash
  node run-benchmark.mjs
  ```

- Run two additional repeat samples (runs 2 and 3), preserving the original run:

  ```bash
  node run-repeats.mjs
  node summarize-repeats.mjs
  ```

- `summarize-repeats.mjs` expects `repeat-scores.json` to exist and prints per-model/per-task variance summaries.
- Run dependency-free harness tests with `npm test` and syntax checks with
  `npm run check`.

There is no build step. Tests use Node's built-in `node:test` module.

## Architecture

- `tasks.json` is the single source of truth for benchmark tasks. It is an array of objects with `id`, `category`, and `prompt`.
- `models.local.json` is the ignored local model configuration;
  `models.example.json` documents its schema.
- `src/harness.mjs` validates tasks, builds jobs, limits concurrency, invokes a
  provider-neutral `generate(job)` function, and persists results.
- `src/providers/ncode.mjs` builds NCode CLI invocations and handles subprocess
  output, errors, and timeouts.
- `src/providers/index.mjs` maps provider names to adapters.
- `src/statistics.mjs` validates and summarizes manually entered scores.
- `src/models.mjs` loads and validates portable model configuration.
- `run-benchmark.mjs` reads `tasks.json`, iterates over all `(task, model)` pairs, and invokes the local `ncode` binary for each.
- `run-repeats.mjs` is nearly identical but handles `run-2/` and `run-3/` subdirectories under `results/`.
- Both runner scripts:
  - Spawn `ncode --print --no-session-persistence --tools "" --effort medium --model <modelId> --output-format json <prompt>`.
  - Set a 5-minute per-task timeout.
  - Run up to 4 jobs concurrently.
  - Skip a job if an existing output file already shows `exitCode: 0`.
  - Write one JSON file per task/model combination into `results/`.
- Provider functions return `exitCode`, `signal`, `stdout`, `stderr`, and
  `error`; the harness owns timestamps, metadata, skip checks, and persistence.
- `summarize-repeats.mjs` computes totals, means, standard deviations, and ranges across repeat runs using `repeat-scores.json`.

## Key files and output layout

- `tasks.json`: task definitions.
- `run-benchmark.mjs`: primary runner (outputs to `results/<task-id>--<model-name>.json`).
- `run-repeats.mjs`: repeat runner (outputs to `results/run-2/` and `results/run-3/`).
- `summarize-repeats.mjs`: aggregate statistics printer.
- `repeat-scores.json`: manually scored totals used by `summarize-repeats.mjs`.
- `results/`: ignored local provider outputs; never commit raw generated data.
- `repeat-scores.example.json`: format for manually entered multi-run scores.

## Important evaluation conventions

- Scoring is out of 10 per task using this rubric:
  - Correctness: 4
  - Completeness and constraint compliance: 2
  - Edge cases and safety: 1.5
  - Test quality: 1
  - Clarity and maintainability: 1
  - Technical reasoning: 0.5
- Keep provider settings, prompts, tool access, and run counts comparable.
- Disclose model-specific differences in context limits, runtime, and provider
  behavior when interpreting results.

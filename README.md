# LLM Coding Benchmark

[![60 tests](https://img.shields.io/github/actions/workflow/status/arjun7965/llm-coding-benchmark/ci.yml?branch=main&event=push&label=60%20tests)](https://github.com/arjun7965/llm-coding-benchmark/actions/workflows/ci.yml?query=branch%3Amain)
[![20 C tests](https://img.shields.io/github/actions/workflow/status/arjun7965/llm-coding-benchmark/c-tests.yml?branch=main&event=push&label=20%20C%20tests)](https://github.com/arjun7965/llm-coding-benchmark/actions/workflows/c-tests.yml?query=branch%3Amain)

A dependency-free Node.js harness for running the same coding tasks against
multiple language models. Models are configured locally, and provider-specific
execution is isolated behind adapters.

## Requirements

- Node.js 22 or newer
- A supported provider runtime; NCode and OpenAI-compatible HTTP are included
- Local access or credentials required by the configured models

Language toolchains such as `rustc`, a C11 compiler, Go, Python, or PostgreSQL
are needed only when compiling or executing answers for the corresponding task.
See `docs/dependencies.md` for the complete validation matrix.

## Quick Start

Create a private model configuration:

```bash
cp models.example.json models.local.json
```

Edit `models.local.json`:

```json
{
  "models": [
    {
      "id": "my-model",
      "provider": "ncode",
      "model": "provider/model-or-local-path",
      "options": {
        "effort": "medium",
        "timeoutMs": 300000
      }
    }
  ]
}
```

Run the benchmark and tests:

```bash
npm run benchmark
npm test
npm run test:c
npm run check
```

Use another configuration without copying it into the repository:

```bash
BENCHMARK_MODELS_FILE=/path/to/models.json npm run benchmark
```

## Tasks and Results

`tasks.json` defines the shared prompts. Each task has a stable lowercase ID,
category, prompt, and optional `targetProfile`. Embedded and firmware categories
require a recognized profile. The harness writes one record per task/model pair
under `results/`.

Select a subset or override execution controls without editing configuration:

```bash
npm run benchmark -- \
  --models local-openai-compatible \
  --tasks embedded-ring-buffer,firmware-state-machine \
  --runs 1,2,3 \
  --concurrency 2 \
  --output results/firmware
```

`--models` and `--tasks` accept comma-separated or repeated exact IDs.
`--models-file` and `--tasks-file` select alternate input documents. Run
`npm run benchmark -- --help` or `npm run benchmark:repeats -- --help` for the
complete interface.

Raw records include a SHA-256 of the task prompt. A changed prompt invalidates
result reuse and prevents stale answers from entering fixture extraction.

Raw outputs are intentionally Git-ignored because generated text can contain
credentials, session metadata, or local paths. Keep raw runs private and publish
only reviewed, sanitized exports.

Create a sanitized projection:

```bash
npm run export:public -- --input results --output public-results
```

Exports containing redactions require explicit review. See
`docs/publishing-results.md` and `schemas/public-result.schema.json`.

## Repeated Runs and Scoring

Generate two additional samples and summarize manually entered scores:

```bash
npm run benchmark:repeats
cp repeat-scores.example.json repeat-scores.json
npm run summarize
```

The default rubric scores correctness, constraint compliance, edge cases,
testing, maintainability, and technical reasoning out of 10. Blind model
identities during initial scoring and disclose differences in provider settings,
tools, context limits, or execution environments.

Task-specific ten-point rubrics are under `docs/benchmarks/`. Machine-readable
contracts are provided in `schemas/tasks.schema.json` and
`schemas/repeat-scores.schema.json`; the summarizer also validates cross-field
requirements such as score-array lengths.

Embedded and firmware expansion is governed by
`docs/embedded/capability-matrix.md` and reusable target profiles in
`docs/embedded/target-assumptions.md`. Profiles are recorded as validation and
result metadata; they do not inject hidden text into model prompts.

## Optional Cross-Compilation

Probe trusted C references for ARMv7-M and RV32 portability:

```bash
npm run cross:check
npm run cross:check -- --target armv7m-bare-metal
```

Unavailable compilers are skipped locally. Pass `--require-tools` to fail when
a compiler is missing. The manual **Cross compilation** GitHub Actions workflow
installs both toolchains and requires both target checks to pass. These checks
compile trusted sources only; they do not link, execute, or validate generated
model code.

## Fixture Scaffolds

Profile-backed tasks have manifests under `fixtures/<task-id>/`. Manifests
declare answer extraction, stable asset paths, required tools, argv-based build
commands, and whether a fixture is an inactive scaffold or executable.

```bash
npm run fixtures:check
```

This validates fixture structure and task/profile references without compiling
or executing model output. See `fixtures/README.md` for the directory contract.

Extract the single expected fenced code block from a successful raw result:

```bash
npm run fixture:extract -- \
  --result results/binary-parser--my-model.json
```

The output path comes from the validated fixture manifest and remains ignored
by Git. Extraction rejects failed or stale-prompt results, ambiguous fences,
unsafe paths, and existing output unless `--force` is explicit. It does not
compile or execute the extracted code. Run `npm run test:c` to verify all public
C fixtures against their trusted references.

## Adding a Provider

Implement an adapter under `src/providers/` that accepts a job and returns:

```js
{
  exitCode,
  signal,
  stdout,
  stderr,
  error,
}
```

Register it in `src/providers/index.mjs`, then reference its provider name in
the local model configuration. Provider options are carried in
`job.modelOptions`.

The included `openai-compatible` provider sends non-streaming Chat Completions
requests to a configured HTTP endpoint. It supports unauthenticated local
servers and environment-based Bearer credentials. See
`docs/providers/openai-compatible.md` for configuration and safety constraints.

## Security Checks

```bash
npm run security:scan
```

CI scans repository content for common credential formats and personal paths.
The scanner reports only location and finding type, never the matched value.

## License

Original code, benchmark prompts, and documentation are licensed under Apache
License 2.0, copyright 2026 Arjun Vinod. Generated outputs are not included in
the repository; see `GENERATED_OUTPUTS.md` for the publication policy.

# LLM Firmware Benchmark

[![75 tests](https://img.shields.io/github/actions/workflow/status/arjun7965/llm-firmware-benchmark/ci.yml?branch=main&event=push&label=75%20tests)](https://github.com/arjun7965/llm-firmware-benchmark/actions/workflows/ci.yml?query=branch%3Amain)
[![50 C checks](https://img.shields.io/github/actions/workflow/status/arjun7965/llm-firmware-benchmark/c-tests.yml?branch=main&event=push&label=50%20C%20checks)](https://github.com/arjun7965/llm-firmware-benchmark/actions/workflows/c-tests.yml?query=branch%3Amain)
[![4 sandbox fixtures](https://img.shields.io/github/actions/workflow/status/arjun7965/llm-firmware-benchmark/sandbox-tests.yml?branch=main&event=push&label=4%20sandbox%20fixtures)](https://github.com/arjun7965/llm-firmware-benchmark/actions/workflows/sandbox-tests.yml?query=branch%3Amain)

A dependency-free Node.js harness for evaluating language models on firmware
and embedded coding tasks. Deterministic host fixtures, mutation tests,
sandboxed execution, and optional cross-compilation support reproducible
validation. General coding tasks remain as an auxiliary comparison suite.

## Requirements

- Node.js 22 or newer
- A supported provider runtime; NCode and OpenAI-compatible HTTP are included
- Local access or credentials required by the configured models

Language toolchains such as `rustc`, a C11 compiler, Go, Python, or PostgreSQL
are needed only when compiling or executing answers for the corresponding task.
See `docs/dependencies.md` for the validation matrix and
`docs/validation-profiles.md` for reusable runtime assumptions.
Exact profile revisions, toolchains, dependencies, sandbox policy, and resource
limits are pinned in `validation-profiles.json`.

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
category, explicit `firmware` or `auxiliary` suite, `validationProfile`, prompt,
and optional `targetProfile`. Firmware-suite tasks require a recognized target
profile. The harness writes one record per task/model pair under `results/`.

Firmware tasks are the primary suite. Auxiliary tasks may rely on manual or
external validation.

Select a subset or override execution controls without editing configuration:

```bash
npm run benchmark -- \
  --models local-openai-compatible \
  --suites firmware \
  --tasks embedded-ring-buffer,firmware-state-machine \
  --runs 1,2,3 \
  --concurrency 2 \
  --output results/firmware
```

`--models`, `--suites`, and `--tasks` accept comma-separated or repeated exact
values. Suite and task filters intersect. `--models-file` and `--tasks-file`
select alternate input documents. Run `npm run benchmark -- --help` or
`npm run benchmark:repeats -- --help` for the complete interface.

Raw records include the validation and target profiles plus a SHA-256 of the
task prompt. A changed prompt or validation profile invalidates result reuse
and prevents stale answers from entering fixture extraction.

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
requirements such as score-array lengths. Firmware-suite tasks use the shared
`firmware-v1` dimensions in `docs/benchmarks/firmware-scoring.md`. Summaries
report combined totals and separate firmware and auxiliary totals using the
task registry; set `BENCHMARK_TASKS_FILE` when scoring an alternate task file.
Each score document pins logical-profile and concrete-environment revisions
and SHA-256 values per task. The summarizer verifies those references against
the task registry before comparing models or runs.

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

## Fixture Validation

Profile-backed tasks have manifests under `fixtures/<task-id>/`. Manifests
declare answer extraction, stable asset paths, required tools, argv-based build
and test commands, and whether a fixture is an inactive scaffold or active.

```bash
npm run fixtures:check
```

This validates fixture structure and task/profile references without compiling
or executing model output. See `fixtures/README.md` for the directory contract
and `docs/answer-contracts.md` for the single-file default and future
multi-file rules.

Extract the single expected fenced code block from a successful raw result:

```bash
npm run fixture:extract -- \
  --result results/binary-parser--my-model.json
```

The output path comes from the validated fixture manifest and remains ignored
by Git. Extraction rejects failed or stale-prompt results, ambiguous fences,
unsafe paths, and existing output unless `--force` is explicit. It does not
compile or execute the extracted code. Run `npm run test:c` to verify all
public C fixtures against their trusted references and confirm that all 23
controlled mutations are rejected. Use `npm run test:mutations` to run only
the mutation checks.

On Linux, validate extracted code in separate compile and test sandboxes:

```bash
npm run fixture:validate -- --task binary-parser
```

This requires Bubblewrap, `prlimit`, and the fixture toolchain. It fails closed
if isolation is unavailable and writes an ignored machine-readable report under
the fixture’s `build/` directory. See
`docs/sandboxed-validation.md` for the isolation boundary and limitations.
Reports include suite, validation-profile, target, and language metadata,
compiler versions, exact argv, binary sizes, normalized outcomes, and
diagnostics.

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

# LLM Firmware Benchmark

[![136 tests](https://img.shields.io/github/actions/workflow/status/arjun7965/llm-firmware-benchmark/ci.yml?branch=main&event=push&label=136%20tests)](https://github.com/arjun7965/llm-firmware-benchmark/actions/workflows/ci.yml?query=branch%3Amain)
[![75 C/C++ checks](https://img.shields.io/github/actions/workflow/status/arjun7965/llm-firmware-benchmark/c-tests.yml?branch=main&event=push&label=75%20C%2FC%2B%2B%20checks)](https://github.com/arjun7965/llm-firmware-benchmark/actions/workflows/c-tests.yml?query=branch%3Amain)
[![10 sandbox fixtures](https://img.shields.io/github/actions/workflow/status/arjun7965/llm-firmware-benchmark/sandbox-tests.yml?branch=main&event=push&label=10%20sandbox%20fixtures)](https://github.com/arjun7965/llm-firmware-benchmark/actions/workflows/sandbox-tests.yml?query=branch%3Amain)

A dependency-free Node.js harness for evaluating language models on firmware
and embedded coding tasks. Deterministic host fixtures, mutation tests,
sandboxed execution, and optional cross-compilation support reproducible
validation. General coding tasks remain as an auxiliary comparison suite.

**[Explore the interactive benchmark guide →](https://arjunvinod.com/llm-firmware-benchmark/)**

The guide explains the methodology, provides a searchable catalog of every
task and rubric, walks through grading, and gives a runnable first-benchmark
path. Its task data is generated from this repository at deploy time.

## Requirements

- Node.js 22 or newer
- A supported provider runtime; Claude Code, Codex, OpenCode, and
  OpenAI-compatible HTTP are included
- Local access or credentials required by the configured models

Language toolchains such as `rustc`, a C11 compiler, Go, Python, or PostgreSQL
are needed only when compiling or executing answers for the corresponding task.
See `docs/dependencies.md` for the validation matrix and
`docs/validation-profiles.md` for reusable runtime assumptions.
Exact profile revisions, toolchains, dependencies, sandbox policy, and resource
limits are pinned in `validation-profiles.json`.

## Benchmark Website

The public guide is a static site in `site/`. Its searchable task and scoring
data is generated directly from `tasks.json`, `validation-profiles.json`,
fixture manifests, and `docs/benchmarks/`, so changes to those sources are
checked during the site build.

Build it locally and open `site-dist/index.html` in a browser:

```bash
npm run site:build
```

The `Deploy GitHub Pages` workflow rebuilds and publishes the site after
relevant changes reach `main`. A repository administrator must select
**GitHub Actions** once under **Settings → Pages → Build and deployment**;
after that, pushes and manual workflow dispatches deploy automatically.

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
      "id": "gpt-5.6-luna",
      "provider": "codex",
      "model": "gpt-5.6-luna",
      "options": {
        "effort": "medium",
        "timeoutMs": 600000
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

Codex models reuse the authentication from `codex login`. The example model
file includes `gpt-5.6-luna`, `gpt-5.6-sol`, and `gpt-5.6-terra`; select them
without running locally configured models:

```bash
npm run benchmark -- \
  --models gpt-5.6-luna,gpt-5.6-sol,gpt-5.6-terra
```

Claude models reuse the authentication from `claude auth login`. Run the
example Claude Code configuration with:

```bash
npm run benchmark -- --models claude-sonnet-5
```

OpenCode models use `provider/model` identifiers and reuse authentication from
`opencode auth login`. Replace the placeholder identifier in the example model
configuration, then run it with:

```bash
npm run benchmark -- --models opencode-example
```

Use another configuration without copying it into the repository:

```bash
BENCHMARK_MODELS_FILE=/path/to/models.json npm run benchmark
```

## Tasks and Results

`tasks.json` defines the shared prompts. Each task has a stable lowercase ID,
category, explicit `firmware` or `auxiliary` suite, `scoringMode`,
`validationProfile`, prompt, and optional `targetProfile`. Firmware-suite tasks
require a recognized target profile. The harness writes one record per
task/model pair under `results/`.

Firmware tasks are the primary suite. A `deterministic` task has a
fixture-owned validator; a `rubric-only` task is manually scored with its
published rubric because a reproducible validator would require an undocumented
service or environment-dependent evidence. See
[`docs/rubric-only-tasks.md`](docs/rubric-only-tasks.md).

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

Raw records include the scoring mode, validation and target profiles, plus a
SHA-256 of the task prompt. Providers can also record a SHA-256 of their fixed
execution context; OpenCode fingerprints its isolated agent and invocation
configuration. A changed prompt, scoring mode, validation profile, or defined
provider configuration invalidates result reuse and prevents stale answers
from entering fixture extraction.

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

For a cross-model calibration pilot, prepare a private identity-blinded packet
inside the ignored `results/` tree, complete its score sheet before opening the
identity key, then validate and summarize the locked scores:

```bash
npm run calibration:blind -- \
  --input results/<pilot> \
  --task <task-id> \
  --output results/<pilot>/blind-scoring
npm run calibration:summarize -- \
  --directory results/<pilot>/blind-scoring
```

The preparation command extracts complete provider answers, rejects failed
samples and normalized literal model/provider identifiers in answer text,
randomizes their order, and writes `packet.json`, `score-sheet.json`, and a
separate `identity-key.json`. It omits the rubric's `## Calibration` section
from the reviewer packet so prior outcomes cannot anchor new scores. The
summary command
fails closed on identity-key, packet, answer, criterion, total, or model/run
mismatches. The packet commits the sealed identity key's SHA-256 before
scoring; the hidden key includes a random 256-bit nonce so the small set of
possible model/run permutations cannot be brute-forced from that commitment.
Post-review remapping is therefore detectable without revealing the mapping.
Keep all three files private; only publish reviewed aggregate findings.
Packet inclusion attests successful provider generation and answer extraction
from its envelope, not fixture answer-contract extraction or deterministic
validation; verify those records separately before scoring.
Before handoff, manually inspect the packet for aliases or stylistic identity
clues that literal identifier screening cannot detect.

The default rubric scores correctness, constraint compliance, edge cases,
testing, maintainability, and technical reasoning out of 10. Blind model
identities during initial scoring and disclose differences in provider settings,
tools, context limits, or execution environments.

Task-specific ten-point rubrics are under `docs/benchmarks/`. Machine-readable
contracts are provided in `schemas/tasks.schema.json` and
`schemas/repeat-scores.schema.json`; the summarizer also validates cross-field
requirements such as exact per-run task coverage. Each run records totals by
task ID, so scores do not depend on the order of the `tasks` array:

```json
"run1": {
  "embedded-ring-buffer": 9.0,
  "firmware-state-machine": 8.5
}
```

Firmware-suite tasks use the shared
`firmware-v1` dimensions in `docs/benchmarks/firmware-scoring.md`. Summaries
report combined totals and separate firmware and auxiliary totals using the
task registry; set `BENCHMARK_TASKS_FILE` when scoring an alternate task file.
Each score document pins the scoring mode for every task and logical-profile
and concrete-environment revisions and SHA-256 values for deterministic tasks.
The summarizer verifies those references against the task registry before
comparing models or runs.

Embedded and firmware expansion is governed by
`docs/embedded/capability-matrix.md` and reusable target profiles in
`docs/embedded/target-assumptions.md`. Profiles are recorded as validation and
result metadata; they do not inject hidden text into model prompts.
New or materially revised tasks must also follow the
[`vendor specification and source policy`](docs/vendor-specifications.md):
prefer original fictional interfaces, record any public specification facts
that influence the task, and never commit confidential or nonredistributable
vendor material.

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

## Optional Hardware-in-the-Loop Validation

The supplemental HIL catalog covers ST NUCLEO-F446RE, NXP FRDM-MCXN947, and
TI LP-MSPM0G3507 boards. Validate its board, probe, toolchain, SDK, license, and
test metadata without touching hardware:

```bash
npm run hil:check
npm run hil:check -- --target stm32-nucleo-f446re --probe-tools
```

Physical lab runs use a common probe, flash verification, reset, UART, GPIO,
and watchdog protocol. Their versioned reports pin the catalog, firmware,
dependencies, hashed probe identity, and evidence digests. HIL remains
supplemental: host mocks are always the required scoring path. See
[`docs/embedded/hardware-in-the-loop.md`](docs/embedded/hardware-in-the-loop.md)
for official board and datasheet links, electrical and flashing safety,
vendor licenses, lab setup, and report validation.

## Fixture Validation

Fixture-backed tasks have manifests under `fixtures/<task-id>/`. Manifests
declare answer extraction, stable asset paths, required tools, argv-based build
and test commands, and whether a fixture is an inactive scaffold or active.

```bash
npm run fixtures:check
```

This validates fixture structure and task/profile references without compiling
or executing model output. See `fixtures/README.md` for the directory contract
and `docs/answer-contracts.md` for the single-file default and opt-in
multi-file rules.

Extract the manifest-declared fenced code or file bundle from a successful raw
result:

```bash
npm run fixture:extract -- \
  --result results/binary-parser--my-model.json
```

Output paths come from the validated fixture manifest and remain ignored by
Git. Bundle files require exact manifest-order headings and language fences and
are committed transactionally. Extraction rejects failed or stale-prompt
results, ambiguous fences, unsafe paths, and existing output unless `--force`
is explicit. It does not compile or execute the extracted code. Run `npm run test:c` to verify all
public C fixtures against their trusted references and confirm that every
controlled C mutation is rejected. Use `npm run test:mutations` to run only
the mutation checks.

The RTOS fixtures are calibrated with `npm run fixture:rtos:self-test`,
`npm run fixture:rtos-scheduler:self-test`, `npm run fixture:rtos-queue:self-test`,
and `npm run fixture:rtos-events:self-test`. Together they cover
priority-inheritance mutexes, rate-monotonic release scheduling, queue/counting-
semaphore handoff, event-flag consumption, bounded waits, and deadlock-safe
mutex ordering through deterministic C11 mocks.

The constrained-memory fixtures are calibrated with
`npm run fixture:ring-buffer:self-test`,
`npm run fixture:static-memory-pool:self-test`,
`npm run fixture:dma-cache:self-test`, and
`npm run fixture:fixed-point:self-test`. The resource-optimization fixture is
calibrated with `npm run fixture:fixed-point-filter:self-test`. Together they cover lock-free
caller-owned ring storage, deterministic static block allocation and alignment,
noncoherent DMA cache-line maintenance, and fixed-point processing under an
explicit stack high-water budget plus symmetric FIR execution under modeled
cycle and numerical-error budgets.

The active Rust stream-decoder fixture is calibrated with
`npm run fixture:rust-decoder:self-test` and validated in CI under pinned
Rust/Cargo 1.87.0 with its attested GCC 13.3.0 linker.

The active concurrency-debug fixture is calibrated with
`npm run fixture:concurrency:self-test` and validated in CI under its pinned,
root-owned Python 3.12.11 runtime.

The active TypeScript singleflight-cache fixture is calibrated with
`npm run fixture:typescript-cache:self-test` and validated in CI with its
attested, read-only Node.js 22.16.0 and TypeScript 5.8.3 installation.

The active backend idempotency fixture is calibrated with
`npm run fixture:backend-idempotency:self-test`. It compiles a two-file
TypeScript/SQL answer with an attested Express and `pg` package tree, then
exercises duplicate HTTP requests against a fresh private PostgreSQL socket.

The active webhook replay security fixture is calibrated with
`npm run fixture:webhook-replay-security:self-test`. It authenticates exact
raw request bytes, verifies secret rotation and cross-instance replay behavior,
and rolls back the event with its outbox action on a PostgreSQL failure.

The active resilient serial-service fixture is calibrated with
`npm run fixture:serial-service:self-test`. It deterministically wraps
GNU/Linux and POSIX device, signal, polling, and termios calls to verify raw
115200-8N1 setup, bounded delivery, graceful SIGINT/SIGTERM shutdown, expected
device-loss recovery, and capped reconnect backoff without physical hardware.

The active supervised process-service fixture is calibrated with
`npm run fixture:process-supervisor:self-test`. It deterministically scripts a
supplied worker launcher plus Linux/POSIX pidfd, bounded `SOCK_SEQPACKET`,
signal, termination, and reaping calls to verify at-least-once delivery,
restart exhaustion/reset, and bounded graceful-to-forced shutdown.

The active ADC threshold/watchdog fixture is calibrated with
`npm run fixture:adc-watchdog:self-test`. It verifies 12-bit threshold
classification, bounded ISR status handling, wrap-safe timeout, explicit fault
recovery, and exact interrupt-state restoration against an opaque ADC0 model.

The active linker-symbol memory-map fixture is calibrated with
`npm run fixture:linker-memory:self-test`. It verifies reset-time linker-range
validation, initialized-data copying, BSS clearing, no-effect rejection, and
half-open image, writable, and stack boundaries against opaque flash/SRAM
models.

The active Cortex-M hard-fault debugging fixture is calibrated with
`npm run fixture:hard-fault-debug:self-test`. It verifies a precise BusFault
diagnosis across a supplied exception frame, SCB snapshot, map, disassembly,
and defective source, then checks the exact one-past boundary repair.

The active PWM synchronized-update fixture is calibrated with
`npm run fixture:pwm:self-test`. It verifies shadow-register boundary timing,
fault-over-update priority, last-safe-duty recovery, event gating, and exact
interrupt-state restoration against an opaque PWM0 model.

The active watchdog-window recovery fixture is calibrated with
`npm run fixture:watchdog-window:self-test`. It verifies exact feed-window
boundaries, persistent reset-cause handling, acknowledgement-gated recovery,
and interrupt-state restoration against an opaque WDT0 model.

The active timer-DMA ownership handoff fixture is calibrated with
`npm run fixture:timer-dma:self-test`. It verifies descriptor ownership,
terminal handoff priority, abort acknowledgement, retained-compare recovery,
and interrupt-state restoration against opaque TIMER0/DMA0 models.

The active timer capture/compare overflow fixture is calibrated with
`npm run fixture:timer-capture:self-test`. It verifies delayed-overflow
timestamp reconstruction, bounded compare arming across a 16-bit wrap, stale
status handling, capture overrun accounting, and interrupt-state restoration.

The active frontend-autocomplete fixture is calibrated with
`npm run fixture:frontend-autocomplete:self-test`. Deterministic jsdom
interaction tests verify its exact debounce boundary, async race handling,
keyboard and mouse behavior, and ARIA relationships under pinned React 18.

The active Go graceful-shutdown fixture is calibrated with
`npm run fixture:go-shutdown:self-test`. Its two-file bundle compiles the
candidate implementation and focused tests under pinned Go 1.24.4 while an
independent public suite remains the authoritative validation gate.

The active property-based testing fixture is calibrated with
`npm run fixture:property-tests:self-test`. It runs a model-authored test module
against the supplied `pathutil` implementation using the attested Python
3.12.11, pytest 8.4.0, and Hypothesis 6.135.9 environment.

On Linux, validate extracted code in separate compile and test sandboxes:

```bash
npm run fixture:validate -- --task binary-parser
```

Host environments require Bubblewrap, `prlimit`, and the fixture toolchain.
The registered `debian-12-x86-64-c11-oci` environment provides a portable C11
alternative. OCI environments require pinned Podman, low-level runtime,
monitor, runtime-configuration, and seccomp contracts and must be selected
explicitly with `--environment debian-12-x86-64-c11-oci`; the validator uses
only the matching digest-pinned image already present locally and never pulls.
Its reviewed recipe, publication evidence, and reproducibility procedure are
under `oci/c11/`.
Both modes fail closed if isolation is unavailable and write an ignored
machine-readable report under the fixture’s `build/` directory. See
`docs/sandboxed-validation.md` for the isolation boundary and limitations.
Reports include suite, validation-profile, target, and language metadata,
toolchain versions, OCI image identity, provenance, runtime configuration and
security evidence when applicable, exact argv, native artifact sizes when
produced, normalized outcomes, and diagnostics.

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

The included `codex` provider runs the Codex CLI non-interactively with saved
CLI authentication, isolated temporary working directories, and a fixed
no-tools policy. See `docs/providers/codex.md` for configuration and benchmark
parity considerations.

The included `claude-code` provider runs Claude Code non-interactively with
saved CLI authentication, isolated temporary working directories, no session
persistence, and a fixed no-tools policy. See
`docs/providers/claude-code.md` for configuration and benchmark parity
considerations.

The included `opencode` provider runs OpenCode non-interactively with saved
provider authentication, isolated temporary working directories, pure mode,
isolated agent configuration, and a fixed no-tools policy. See
`docs/providers/opencode.md` for configuration, NDJSON output handling, and
benchmark parity considerations.

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

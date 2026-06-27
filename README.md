# LLM Coding Benchmark

A dependency-free Node.js harness for running the same coding tasks against
multiple language models. Models are configured locally, and provider-specific
execution is isolated behind adapters.

## Requirements

- Node.js 22 or newer
- A supported provider runtime; NCode is included
- Local access or credentials required by the configured models

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
npm run check
```

Use another configuration without copying it into the repository:

```bash
BENCHMARK_MODELS_FILE=/path/to/models.json npm run benchmark
```

## Tasks and Results

`tasks.json` defines the shared prompts. Each task has a stable lowercase ID,
category, and prompt. The harness writes one record per task/model pair under
`results/`.

Raw outputs are intentionally Git-ignored because generated text can contain
credentials, session metadata, or local paths. Keep raw runs private and publish
only reviewed, sanitized exports.

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

## License

Original code, benchmark prompts, and documentation are licensed under Apache
License 2.0, copyright 2026 Arjun Vinod. Generated outputs are not included in
the repository; see `GENERATED_OUTPUTS.md` for the publication policy.

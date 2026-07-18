# Rubric-Only Task Policy

Every task declares a `scoringMode` in `tasks.json`:

- `deterministic` means a fixture-owned, pinned, isolated validator supplies
  executable evidence alongside the task rubric.
- `rubric-only` means the task is evaluated solely with its published
  ten-point rubric. It must not claim executable validation evidence.

Choose the mode before collecting model output. A task may not use one mode for
one model or run and another for a comparison peer.

## When Rubric-Only Is Required

Set `scoringMode` to `rubric-only` rather than inventing a fixture when either
condition applies:

1. **Undocumented service.** Correct behavior depends on a vendor, internal,
   licensed, or otherwise unavailable service whose protocol, version,
   lifecycle, state, and failure behavior cannot be published and reproduced.
2. **Environment-dependent scoring.** The score would vary materially with
   uncontrolled external state such as account data, time, region, billing
   tier, real hardware revision, network behavior, or an unavailable service.

An executable fixture is allowed only after its required service contract is
documented, versions and dependencies are pinned, initial state and lifecycle
are fixture-owned, network access is disabled, and public tests and controlled
mutations are calibrated in the declared validation environment. A surrogate
that omits behavior needed to score the prompt is not a deterministic fixture.

## Required Metadata

A rubric-only task records one or both machine-readable reasons and a concise
public rationale:

```json
{
  "scoringMode": "rubric-only",
  "rubricOnlyReasons": ["undocumented-service"],
  "rubricOnlyRationale": "The required vendor simulator is not publicly documented or distributable."
}
```

`deterministic` tasks must omit the rubric-only fields. Reclassifying a task
requires a documented rationale, a prompt-hash review, and calibration before
any new results are compared with older ones.

## Enforcement and Scoring

The task loader validates this metadata. Fixture validation requires every
`deterministic` task to have a fixture and rejects fixture directories for
`rubric-only` tasks. Extraction, sandbox validation, mutation testing, and
fixture reports therefore cannot be used to manufacture executable evidence
for a rubric-only task.

Raw and public result records carry the scoring mode. The public exporter uses
the conservative `rubric-only` mode for legacy raw records that predate this
metadata. Re-run legacy work before treating it as deterministic evidence.

Repeated-score documents pin every task's scoring mode. They include a
profile/environment validation contract only for `deterministic` tasks;
rubric-only tasks must not receive one. Grade rubric-only answers blind using
the task's published rubric, record behavior-specific deductions, and disclose
the stated rationale with the score set. Do not substitute ad hoc service
access, local account data, or environment-specific test output for those
published criteria.

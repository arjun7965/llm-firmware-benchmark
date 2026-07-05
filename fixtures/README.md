# Task Fixtures

Each fixture lives at `fixtures/<task-id>/` and contains:

```text
manifest.json
mutations.json # Controlled defects the public tests must reject
starter/       # Supplied source or headers
mocks/         # Deterministic hardware, HAL, clock, or OS fakes
tests/public/  # Tests disclosed to benchmark participants
reference/     # Trusted implementation used to verify fixture behavior
scripts/       # Fixture-local validation helpers
generated/     # Extracted model code; ignored
build/         # Compiler and test output; ignored
```

`manifest.json` follows `schemas/fixture-manifest.schema.json` and its
`validationProfile` and `targetProfile` must match `tasks.json`. Commands and
per-tool `toolVersionArgs` are stored as argv arrays and must be executed
without a shell. Version arguments must cover exactly the tools named by
`requiredTools`; for example, use `["--version"]` for `cc` and `["version"]`
for Go. A `scaffold` manifest defines an incomplete interface. An `active`
manifest has verified extraction, compile, and test commands.

Fixture manifests, validation reports, and public result records use schema
version 1.3. Mutation catalogs remain at version 1.2 because their contract did
not change.

Run `npm run fixtures:check` to validate task/profile references, manifests,
safe paths, and tracked directory structure. This command validates fixture
metadata only; it does not execute compiler commands.

`mutations.json` follows `schemas/fixture-mutations.schema.json`. Every active C
fixture supplies exact, single-match source substitutions derived from its
trusted reference. `npm run test:mutations` requires each mutant to compile
and then fail the public tests; compilation failures are invalid mutations,
not successful detections. `npm run test:c` runs trusted references and
mutations. Mutation tests never use extracted model output.

Extract one successful raw result with:

```bash
npm run fixture:extract -- --result results/<task-id>--<model-id>.json
```

The extractor unwraps provider output, requires exactly one nonempty fence with
the manifest language, and writes only its contents to the ignored
`generated/` path. It rejects failed, mismatched, or stale-prompt results,
malformed fences, oversized content, symlinked output paths, and existing
output. Use `--force` only when intentionally replacing a previous extraction.
Extraction never compiles or executes model output.

Validate the extracted answer in isolated compile and test sandboxes:

```bash
npm run fixture:validate -- --task <task-id>
```

This Linux-only command requires Bubblewrap, `prlimit`, and the manifest
toolchain. It fails rather than running on the host when isolation is
unavailable. Reports are written to the ignored `build/validation-report.json`;
they include toolchain versions, suite and target metadata, exact argv,
artifact sizes, outcomes, and diagnostics. See `docs/sandboxed-validation.md`.

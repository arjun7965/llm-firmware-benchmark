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

Fixture-backed tasks currently use the single-file
`markdown-fenced-code` answer contract. Multi-file tasks remain rubric-only
until the opt-in bundle contract is implemented. See
`docs/answer-contracts.md` for the per-task decisions and activation
requirements.

`manifest.json` follows `schemas/fixture-manifest.schema.json` and its
`validationProfile` and `targetProfile` must match `tasks.json`. Commands and
per-tool `toolVersionArgs` are stored as argv arrays and must be executed
without a shell. Version arguments must cover exactly the tools named by
`requiredTools`; for example, use `["--version"]` for `cc` and `["version"]`
for Go. Every tool must be required by the logical profile, and each version
probe must match every concrete environment supported by that profile in
`validation-profiles.json`. `requiredTools` and `toolVersionArgs` must cover
the profile's complete toolchain set so validation reports attest every tool
included in the environment fingerprint. A `scaffold` manifest defines an
incomplete interface. An `active` manifest has verified extraction, compile,
and test commands.

The current sandbox runner accepts active fixtures only for the native-binary
profiles `c11-host`, `go-std`, and `stable-rust`. Dependency-bearing,
interpreter, and service fixtures must remain scaffolds until their exact
packages and test runtimes can be verified and mounted in the test namespace.

Fixture manifests and public result records use schema version 1.3. Validation
reports use version 1.5, and mutation catalogs remain at version 1.2.

Run `npm run fixtures:check` to validate task/profile references, manifests,
safe paths, and tracked directory structure. This command validates fixture
metadata only; it does not execute compiler commands.

`mutations.json` follows `schemas/fixture-mutations.schema.json`. Every active
fixture supplies exact, single-match source substitutions derived from its
trusted reference. `npm run test:mutations` rewrites the fixture-declared
answer source path and shared `build/` artifacts into a temporary directory,
then runs the manifest's compile and test argv without a shell. Each mutant
must compile for that fixture's language and then fail the public tests;
compilation failures are invalid mutations, not successful detections.
Mutation tests never use extracted model output.

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
logical-profile and concrete-environment revisions and fingerprints, artifact
sizes, outcomes, and diagnostics.
See `docs/sandboxed-validation.md`.

# Task Fixtures

Each fixture lives at `fixtures/<task-id>/` and contains:

```text
manifest.json
starter/       # Supplied source or headers
mocks/         # Deterministic hardware, HAL, clock, or OS fakes
tests/public/  # Tests disclosed to benchmark participants
reference/     # Trusted implementation used to verify fixture behavior
scripts/       # Fixture-local validation helpers
generated/     # Extracted model code; ignored
build/         # Compiler and test output; ignored
```

`manifest.json` follows `schemas/fixture-manifest.schema.json`. Commands are
stored as argv arrays and must be executed without a shell. A `scaffold`
manifest defines the intended extraction and build interface but is not ready
for execution. An `active` manifest must have working assets and commands.

Run `npm run fixtures:check` to validate task/profile references, manifests,
safe paths, and tracked directory structure. This command validates fixture
metadata only; it does not execute compiler commands.

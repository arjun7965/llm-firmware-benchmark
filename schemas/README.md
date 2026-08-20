# Schema Evolution

Versioned schemas declare their data-contract version through
`properties.schemaVersion.const`. Breaking changes, including new required
fields, require a version bump in the schema and every producer, validator,
fixture, test, and document that references it.

`contract-fingerprints.json` is append-only history. Each value is the SHA-256
of the schema after recursively sorting object keys and serializing compact
JSON. When a versioned contract changes:

1. bump its `schemaVersion` value;
2. update all producers and consumers;
3. append the new version and fingerprint without modifying older entries; and
4. run `npm test`.

`test/schema-evolution.test.mjs` discovers every versioned schema, requires a
fingerprint entry for its current version, and fails when the schema content
changes without a matching version entry.

`validation-profiles.schema.json` defines the logical validation profiles and
concrete execution environments pinned in the repository root. Fixture
validation reports preserve both selected revisions and canonical SHA-256
values. The append-only `validation-profile-fingerprints.json` file prevents
published profile or environment revisions from being modified without
detection.

`repeat-scores.schema.json` records a scoring mode for every scored task and a
pinned profile/environment pair for each deterministic task. Runtime validation
additionally checks those values against the task registry and requires every
model run to record exactly one 0–10 total under each declared task ID because
JSON Schema cannot express those cross-document relationships.

`hil-targets.schema.json` defines the supplemental STM32, NXP, and TI lab
catalog. `hil-validation-report.schema.json` records content-addressed HIL
evidence without raw probe identities. Runtime validation additionally pins a
report to the canonical catalog fingerprint, checks target-specific dependency
names, requires the common test order, and derives aggregate success from all
six outcomes. HIL contracts never participate in benchmark scoring.

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

# Repository Publication TODO

## Security and Data Hygiene

- [ ] Implement a sanitized public-result exporter that retains answer text and reproducibility metadata while removing credentials, home paths, session IDs, UUIDs, and unnecessary execution details.
- [ ] Add sanitizer tests with password, API-token, private-key, and local-path canaries.
- [ ] Add automated secret scanning to CI and optionally pre-commit checks.
- [ ] Document a review process for separately publishing generated outputs.

## Benchmark Extensibility

- [ ] Add an OpenAI-compatible HTTP provider adapter.
- [ ] Add CLI filters for models, tasks, runs, concurrency, and output location.
- [ ] Add task-specific documentation and scoring criteria under `docs/benchmarks/`.
- [ ] Define and validate a stable public result schema.
- [ ] Add machine-readable task and score schemas.

## Repository Operations

- [ ] Confirm GitHub Actions passes after the first push.
- [ ] Add issue and pull-request templates if outside contributions increase.

# Repository Publication TODO

## Security and Data Hygiene

- [x] Implement a sanitized public-result exporter that retains answer text and reproducibility metadata while removing credentials, home paths, session IDs, UUIDs, and unnecessary execution details.
- [x] Add sanitizer tests with password, API-token, private-key, and local-path canaries.
- [x] Add automated secret scanning to CI.
- [x] Document a review process for separately publishing generated outputs.

## Benchmark Extensibility

- [ ] Add an OpenAI-compatible HTTP provider adapter.
- [ ] Add CLI filters for models, tasks, runs, concurrency, and output location.
- [ ] Add task-specific documentation and scoring criteria under `docs/benchmarks/`.
- [x] Define and validate a stable public result schema.
- [ ] Add machine-readable task and score schemas.

## Repository Operations

- [x] Confirm GitHub Actions passes after the first push.
- [ ] Add issue and pull-request templates if outside contributions increase.

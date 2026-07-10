# Repository Publication TODO

## Project Scope and Positioning

- [x] Position the repository as a firmware-focused benchmark with auxiliary
  general-coding tasks.
- [x] Rename the repository to `llm-firmware-benchmark`; update package and
  schema identifiers, badges, the local remote, and migration documentation.
- [x] Audit external integrations that may still rely on the old repository URL
  instead of GitHub's redirect.

## Security and Data Hygiene

- [x] Implement a sanitized public-result exporter that retains answer text and reproducibility metadata while removing credentials, home paths, session IDs, UUIDs, and unnecessary execution details.
- [x] Add sanitizer tests with password, API-token, private-key, and local-path canaries.
- [x] Add automated secret scanning to CI.
- [x] Document a review process for separately publishing generated outputs.

## Benchmark Extensibility

- [x] Add an OpenAI-compatible HTTP provider adapter.
- [x] Add CLI filters for models, tasks, runs, concurrency, and output location.
- [x] Add task-specific documentation and scoring criteria under `docs/benchmarks/`.
- [x] Define and validate a stable public result schema.
- [x] Add machine-readable task and score schemas.

## Auxiliary Task Validation Roadmap

The suite boundary is established. Define hosted validation profiles next, then
add auxiliary fixtures after the vendor-specification policy and current
firmware validation work.

### Priority 1 — Suite and Validation Metadata

- [x] Add explicit `suite` metadata (`firmware` or `auxiliary`) to tasks,
  schemas, runtime validation, results, and public exports, with a documented
  backward-compatible default for historical records.
- [x] Add CLI suite filtering and report firmware and auxiliary scores
  separately without changing task IDs or historical score interpretation.
- [x] Add a `validationProfile` field for hosted runtime/toolchain assumptions;
  keep `targetProfile` for architecture, ABI, and hardware assumptions.
- [x] Pin hosted profiles such as `stable-rust`, `python3-stdlib`,
  `node-typescript`, and `go-std`, including tool versions, dependencies,
  resource limits, and sandbox requirements.
- [x] **Important:** Separate logical validation profiles from concrete Linux
  environments. Detect `/etc/os-release` and architecture before validation,
  select an exact supported environment with environment-specific tool pins,
  record the matched environment in reports, and require the same environment
  for scored comparisons. Evaluate digest-pinned OCI images as the preferred
  path for reproducible validation across host distributions.
  - [x] Fail closed when the current pinned OS release or architecture does not
    match the validation host.
- [x] Make scoring-profile selection explicit so hosted auxiliary tasks never
  inherit `firmware-v1` merely because validation metadata is present.
- [x] Decide whether each task returns one source file or uses a validated
  multi-file answer contract before changing prompts or extraction behavior.
- [x] Generalize mutation execution beyond C while retaining exact,
  compile-valid mutations and task-declared argv commands.
- [x] Add verified lockfile installations or digest-pinned images for
  validation profiles with npm or PyPI dependencies. Keep the sandbox runner
  fail-closed for nonempty dependency sets until their installed versions can
  be attested.
- [x] Add profile-approved test-runtime mounts and command contracts for
  interpreter and service profiles. Keep those fixtures scaffold-only until
  the test namespace can execute their pinned runtime.

### Priority 2 — Dependency-Light Auxiliary Fixtures

For each task below, add starter assets, a trusted reference, public tests,
controlled mutations, sandbox commands, dependency documentation, and
calibration before marking the fixture active:

1. [ ] `rust-stream-decoder`
   - [x] Add the API contract, trusted reference, public tests, seven
     controlled mutations, and sandbox command scaffold.
   - [ ] Execute and calibrate the scaffold under pinned Rust/Cargo 1.87.0,
     then mark it active.
2. [ ] `concurrency-debug`
   - [x] Add the exact `Pool` contract, trusted reference, public race tests,
     twelve controlled mutations, and sandbox command scaffold.
   - [ ] Execute and calibrate the scaffold under pinned Python 3.12.11 in
     the sandbox namespace, then mark it active.
3. [ ] `typescript-singleflight-cache`
4. [ ] `go-graceful-shutdown`

Keep validator-only toolchains outside the dependency-free root harness and
record exact versions in validation reports.

### Priority 3 — Specialized Validation Environments

- [ ] Validate `testing-property-based` with isolated pytest/Hypothesis
  dependencies and a controlled set of defective implementations.
- [ ] Design browser-based React/TypeScript validation for
  `frontend-autocomplete`, including deterministic timers, accessibility, and
  interaction checks.
- [ ] Design isolated PostgreSQL validation for `backend-idempotency`,
  `postgres-pagination`, and `webhook-replay-security`, including migrations,
  concurrency, cleanup, and fixed server versions.
- [ ] Keep tasks rubric-only when deterministic execution would require
  undocumented services or introduce environment-dependent scoring.

## Embedded and Firmware Benchmark Expansion

### Task Coverage

- [x] Define a capability matrix spanning bare-metal, RTOS, embedded Linux,
  drivers, protocols, debugging, safety, security, and optimization.
- [ ] Add bare-metal tasks for register access, startup code, linker behavior,
  memory maps, and interrupt-vector configuration.
  - [x] Start with a fictional timer peripheral using a complete mock MMIO
    register map and deterministic host-side validation.
- [ ] Add peripheral-driver tasks for GPIO, UART, SPI, I2C, ADC, PWM, timers,
  DMA, and watchdogs using documented mock registers.
- [ ] Add interrupt and concurrency tasks covering `volatile`, atomics, critical
  sections, race conditions, ISR-safe APIs, and deferred work.
- [ ] Add RTOS tasks for scheduling, queues, mutexes, semaphores, event flags,
  priority inversion, deadlocks, and bounded latency.
- [ ] Add constrained-memory tasks for ring buffers, static allocation, memory
  pools, stack usage, alignment, cache coherency, and fixed-point arithmetic.
- [ ] Add protocol tasks for framing, stateful parsing, timeouts, CRCs, CAN,
  Modbus, BLE-style packets, and malformed-input recovery.
- [ ] Add reliability tasks for watchdog recovery, brownouts, fault handlers,
  persistent state, idempotent initialization, and safe-mode operation.
- [ ] Add bootloader and update tasks covering image validation, rollback,
  interrupted updates, version checks, and secure-boot boundaries.
- [ ] Add power-management and real-time tasks involving sleep states, wake
  sources, clock changes, deadlines, jitter, and execution budgets.
- [ ] Add debugging tasks based on compiler diagnostics, traces, register dumps,
  map files, disassembly, and deliberately defective firmware.
- [ ] Add embedded C and C++ review tasks covering undefined behavior, integer
  conversion, ownership, RAII, portability, and MISRA-style constraints.
- [ ] Add firmware-security tasks for untrusted input, debug interfaces, secret
  handling, update authentication, memory protection, and fault injection.

### Harness and Evaluation

- [x] Define explicit target assumptions for every task: architecture, ABI,
  endianness, compiler, language version, RTOS, and available hardware APIs.
- [x] Store embedded `targetProfile` metadata in `tasks.json`; validate profile
  references through the task schema and runtime validation instead of a
  test-local mapping.
- [x] Define stable per-task fixture directories and a validated manifest/build
  command contract.
- [ ] Build deterministic host-side hardware mocks and simulators so core tasks
  require no physical board or proprietary SDK.
  - [x] Add a deterministic clock/I2C mock and trusted self-test for
    `firmware-state-machine`.
  - [x] Add deterministic full-capacity and counter-wrap tests for
    `embedded-ring-buffer`.
- [ ] Add optional hardware-in-the-loop validation for representative STM32,
  NXP, and TI boards after the mock-MMIO tasks stabilize. Keep host mocks as the
  required scoring path, and document each board, debug probe, toolchain, SDK
  dependency, and license separately.
- [x] Add optional cross-compilation checks for representative ARM and RISC-V
  targets while keeping the default test suite dependency-free.
- [x] Populate fixture starter code, public tests, mocks, extraction helpers,
  and executable validation commands.
  - [x] Add a stable API, trusted reference, and deterministic public self-test
    for `binary-parser`.
  - [x] Add strict provider-neutral fenced-code extraction with safe generated
    output handling.
  - [x] Add fail-closed Bubblewrap compilation and execution with resource
    limits and machine-readable reports.
- [x] Add mutation-based tests for edge cases that prose-only scoring would
  miss, with controlled catalogs for every active C fixture.
- [x] Define firmware-specific scoring for correctness, bounded resource use,
  timing behavior, concurrency safety, fault recovery, portability, and clarity.
- [x] Record toolchain versions, compile flags, target metadata, diagnostics,
  binary size, and test outcomes in machine-readable validation reports.
- [ ] Document when vendor specifications are summarized, and avoid embedding
  confidential SDK content or restrictively licensed source.
- [ ] Pilot and calibrate each task across several model families before adding
  it to the scored benchmark set.

## Repository Operations

- [x] Confirm GitHub Actions passes after the first push.
- [ ] Add issue and pull-request templates if outside contributions increase.

# Repository Publication TODO

## Project Scope and Positioning

- [ ] Decide whether to position the repository as a firmware-focused benchmark
  with auxiliary general-coding tasks. If adopted, revisit the repository name,
  README, task-suite metadata, CLI filtering, and separate score reporting
  before changing existing benchmark semantics.
  - [ ] If renamed, document the GitHub rename, update local remotes and
    hardcoded badge/link URLs, and audit GitHub Pages, hosted Actions, and
    external integrations that may not follow redirects.

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

## Embedded and Firmware Benchmark Expansion

### Task Coverage

- [x] Define a capability matrix spanning bare-metal, RTOS, embedded Linux,
  drivers, protocols, debugging, safety, security, and optimization.
- [ ] Add bare-metal tasks for register access, startup code, linker behavior,
  memory maps, and interrupt-vector configuration.
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
- [x] Add optional cross-compilation checks for representative ARM and RISC-V
  targets while keeping the default test suite dependency-free.
- [ ] Populate fixture starter code, public tests, mocks, extraction helpers,
  and executable validation commands.
  - [x] Add a stable API, trusted reference, and deterministic public self-test
    for `binary-parser`.
  - [x] Add strict provider-neutral fenced-code extraction with safe generated
    output handling.
- [ ] Add private or mutation-based tests for edge cases that prose-only scoring
  would miss.
- [ ] Define firmware-specific scoring for correctness, bounded resource use,
  timing behavior, concurrency safety, fault recovery, portability, and clarity.
- [ ] Record toolchain versions, compile flags, target metadata, diagnostics,
  binary size, and test outcomes in machine-readable results.
- [ ] Document when vendor specifications are summarized, and avoid embedding
  confidential SDK content or restrictively licensed source.
- [ ] Pilot and calibrate each task across several model families before adding
  it to the scored benchmark set.

## Repository Operations

- [x] Confirm GitHub Actions passes after the first push.
- [ ] Add issue and pull-request templates if outside contributions increase.

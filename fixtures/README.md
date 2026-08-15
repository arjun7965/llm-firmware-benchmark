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

Only tasks with `scoringMode: "deterministic"` may have a fixture directory;
every deterministic task must have one. A `rubric-only` task has no manifest,
extraction, mutation, or sandbox contract. See
`../docs/rubric-only-tasks.md`.

Fixture-backed tasks use the single-file `markdown-fenced-code` contract by
default. The opt-in `markdown-file-bundle` contract validates a manifest-owned
ordered file set before an atomic directory replacement. See
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
included in the environment fingerprint. Profiles may also declare
test-runtime command contracts for interpreter or service fixtures; scaffold
manifests for those profiles must use an approved command prefix. A
`scaffold` manifest defines an incomplete interface. An `active` manifest has
verified extraction, compile, and test commands.

Deterministic firmware fixtures have an additional host-coverage invariant.
They must be active, resolve to hosted validation environments, and include a
nonempty `mocks/README.md` that identifies the deterministic boundary. If the
directory contains mock or simulator assets beyond that README, at least one
manifest command must consume the `mocks/` directory. A fixture that needs no
runtime mock documents why its caller-owned inputs or immutable evidence are
sufficient instead. `npm run fixtures:check` enforces this policy and reports
the firmware coverage totals. See
`../docs/embedded/host-validation.md` for the completed audit.

The active `rust-stream-decoder` fixture has a complete API, trusted reference,
public tests, and controlled mutations calibrated under pinned Rust/Cargo
1.87.0 with its GCC 13.3.0 linker in the sandbox namespace. Its trusted test
supervisor requires a randomized child-side completion token so successful
initialization-time exits cannot bypass the public tests.

The active `concurrency-debug` fixture has a complete `Pool` API, trusted
reference, subprocess-isolated race tests, and controlled mutations calibrated
under the pinned Python 3.12.11 sandbox environment.

The active `typescript-singleflight-cache` fixture has a complete generic API,
trusted reference, deterministic fake-clock tests, and controlled mutations.
Its pinned npm package closure is hashed and mounted read-only for compilation;
the compiled tests execute with the pinned Node.js runtime in a separate
sandbox without the package tree.

The active `frontend-autocomplete` fixture has an exact default-exported prop
contract, a trusted React component, deterministic fake-timer/jsdom interaction
tests, and controlled async, keyboard, selection, feedback, and ARIA mutations.
Its attested npm tree is mounted read-only for compilation and test execution.

The active `go-graceful-shutdown` fixture has an exact server-module API, trusted
reference, deterministic lifecycle tests, and controlled mutations. Its test
supervisor requires a child-side completion token so candidate package
initialization cannot exit successfully before the validator suite runs. The
bundle preserves and separately runs the model-authored Go tests only after the
public lifecycle suite succeeds.

The active `testing-property-based` fixture supplies `pathutil.normalize_path`
and validates the model-authored pytest/Hypothesis module. Calibration keeps a
trusted test answer fixed while staging twelve controlled defective
implementations, exercising the mutation runner's supplied-input mode.

The active `postgres-pagination` fixture uses a two-file SQL bundle, a supplied
orders table, strict tenant/filter-bound JSON cursors, and deterministic
keyset/index tests. The runner initializes a fresh PostgreSQL 16.9 data
directory for every compile or test phase, exposes only its private Unix
socket to the candidate sandbox, and tears down the service afterward.

The active `backend-idempotency` fixture uses a two-file TypeScript/SQL bundle.
It mounts its attested Express and `pg` package tree read-only, starts the
candidate app on an in-namespace Unix socket, and exposes only the fresh
PostgreSQL service socket across the server/candidate boundary. Its public
tests cover concurrent requests, raw-byte request binding, replay behavior,
user scoping, validation, and transactional rollback.

The active `webhook-replay-security` fixture uses the same isolated
Node/PostgreSQL boundary for a TypeScript handler and SQL schema bundle. Its
public tests authenticate exact raw bytes before JSON parsing, exercise secret
rotation and concurrent deliveries across two app instances, and verify that
the event and its one outbox row roll back together.

The active `secure-maintenance-command` fixture uses an opaque SEC0 boundary
for lifecycle, physical presence, challenge generation, separate debug/update
verification, and gate publication. Its public tests cover exact unaligned
little-endian frames, replay and lockout semantics, challenge expiry, and the
rule that debug unlock cannot authorize updates. Open-gate initialization and
published-authorization denial cases make fail-closed writes directly
observable rather than relying on reset defaults.

The active `mpu-fault-containment` fixture uses opaque MPU0/security-controller
boundaries and deterministic event traces. Its public tests cover the exact
four-region Cortex-M3 policy, safe-first initialization, readback and injected
fault rejection, containment-before-clear ordering, latched runtime failure,
and exact interrupt-state restoration. Injection spans every programming and
readback boundary, with each region field corrupted independently.

The active `rtos-priority-inversion` fixture uses a fixture-owned C11 RTOS
mock with low-priority telemetry, medium-priority diagnostics, and
high-priority safety contexts. Its public tests verify priority donation,
bounded safety acquisition, error propagation, and initialization recovery.

The active `rtos-periodic-scheduler` fixture models deterministic
rate-monotonic control and telemetry releases with fresh relative deadlines,
late-dispatch collapse, failure retry, and wrap-safe ticks. The active
`rtos-queue-semaphore` fixture models a four-item FIFO and matching counting
semaphore, checking send/give and take/receive order, capacity, and bounded
worker waits. The active `rtos-event-flags-deadlock` fixture models event-bit
consumption plus one global configuration-before-actuator mutex order, bounded
contention, and cleanup after apply failures.

The active constrained-memory fixtures retain `embedded-ring-buffer` for
lock-free caller-owned ring storage, add `static-memory-pool` for 16-byte-
aligned fixed blocks and deterministic exhaustion/reuse, add
`dma-cache-coherency` for cache-line rounded noncoherent DMA ordering, and add
`fixed-point-stack-budget` for Q-format arithmetic with a caller-owned stack
watermark and bounded batch processing. The active
`fixed-point-filter-optimization` fixture adds an opaque deterministic cost
model for a symmetric seven-tap Q1.15 FIR, checking four-MAC execution, a
28-cycle modeled budget, and a one-LSB numerical-error bound. Complete operand
traces and commit-time state validation reject dummy work and early publication
even when a candidate attempts to roll back a failed transaction.

The active `mixed-c-cpp-mmio-safety-review` fixture compiles its opaque C11
MMIO mock separately from the C++17 answer and tests. Immutable defective
evidence drives fixed-line language-safety findings; ordered accessor calls,
checked transfer-count conversion, move-only RAII cleanup, self-move, and two
independent peripheral instances are validated by public tests and mutations.

The active protocol fixtures add Modbus RTU silence-framed stateful request
decoding with CRC recovery, bounded segmented classic-CAN transport payloads
with sequence/DLC/timeout checks, and advertiser-bound BLE-style fragments with
defensive advertising-data parsing. They complement the existing stateless
binary parser and controller-level CAN recovery fixtures.

The active `uart-interrupt-driver` fixture uses accessor-instrumented
fictional UART0 MMIO with a deterministic interrupt-mask model. Its public
tests verify initialization ordering, full-capacity RX/TX queues, bounded
one-byte-per-direction ISR work, overflow accounting, error acknowledgement,
and restoration of the caller's interrupt state.

The active `spi-dma-transfer` fixture uses opaque accessor-instrumented SPI0
and DMA0 models with deterministic full-duplex data movement and an
interrupt-mask boundary. Its public tests verify initialization, full-capacity
descriptor setup, RX-before-TX ordering, stale-status acknowledgement, split
completion, error-priority teardown, recovery, and exact interrupt restoration.

The active `can-controller-recovery` fixture uses an opaque deterministic CAN0
mailbox and status model with a non-nested interrupt boundary. Its public tests
verify classic-CAN frame bounds, initialization and mailbox programming order,
TX error priority, one-slot RX draining/overflow accounting, bus-off
containment, recovery-ready gating, result consumption, and exact interrupt
restoration.

The active `interrupt-deferred-work` fixture uses an opaque volatile interrupt
latch with two priority-ordered ISR sources and deterministic nested-preemption
injection. Its public tests verify atomic source-bit publication, source-local
acknowledgement, deferred-work coalescing, stale-latch clearing, foreground
reconfiguration, and exact interrupt-state restoration.

The active `interrupt-vector-configuration` fixture uses a linker-addressed RAM
vector table with deterministic SCB/NVIC, synchronization-barrier, and
interrupt-mask models. Its public tests verify startup ordering, all table
entries, invalid no-side-effect behavior, reinitialization, runtime IRQ-slot
updates, stale-pending clearing, and exact interrupt restoration.

The active `linker-memory-map` fixture uses opaque linker-symbol, flash, and
SRAM models. Its public tests verify validated reset-time image/data/BSS/stack
layout, initialized-data copying, BSS clearing, no-effect rejection, and
half-open image, writable, and stack address boundaries.

The active `cortex-m-hard-fault-debug` fixture supplies an immutable Cortex-M3
exception frame, SCB register snapshot, linker map, disassembly, and defective
source excerpt. Its public tests verify the exact cross-evidence diagnosis,
Thumb-address symbolization, one-past effective address, and repaired log-store
boundaries without requiring target hardware.

The active `compiler-trace-regression-debug` fixture supplies an immutable
compiler narrowing diagnostic, DMA-planner execution trace, and defective
source excerpt. Its public tests verify the exact cross-evidence diagnosis,
full-width remaining-byte arithmetic, bounded chunk sequences, state
invariants, and stable completion without accessing DMA hardware.

The active `i2c-controller-recovery` fixture uses an opaque deterministic I2C0
model. Its public tests verify initialization, stale-status clearing, bounded
START/address/data ordering, terminal-result consumption, arbitration-loss
recovery without STOP, bus-error/NACK recovery, and wrap-safe timeout aborts.

The active `gpio-edge-debounce` fixture uses an opaque deterministic GPIO0
model with active-low input, edge and wake latches, and an interrupt-mask
boundary. Its public tests verify initialization ordering, both edge
directions, bounce restart, wrap-safe deadlines, retained late edges, wake
recovery, stale-latch clearing, and exact interrupt restoration.

The active `adc-threshold-watchdog` fixture uses an opaque deterministic ADC0
model with end-of-conversion, analog-watchdog, and overrun latches. Its public
tests verify threshold configuration, one-sample terminal handling,
status-priority rules, wrap-safe foreground timeout, fault reset, event
consumption, and exact interrupt restoration.

The active `pwm-synchronized-update` fixture uses an opaque deterministic PWM0
model with shadow/active registers, controlled period boundaries, and fault
latches. Its public tests verify programming order, deferred duty application,
fault-over-update priority, last-safe-duty recovery, event consumption, and
exact interrupt restoration.

The active `watchdog-window-recovery` fixture uses an opaque deterministic
WDT0 model with a feed window, persistent reset-cause latch, timeout reset, and
exact foreground interrupt-state restoration. Its public tests verify boot
recovery, early-feed rejection, reset detection, acknowledgement-gated
recovery, and invalid-call boundaries.

The active reliability fixtures add PWR0 brownout containment with retained
safe-mode state and hysteresis, fault-frame crash capture with checksum-protected
records and explicit clearing, and SYSTEM0 idempotent initialization with
conflict rejection and retained safe boot behavior.

The active boot/update fixtures add BOOT0 signed-image admission with strict
structural, version, digest, and signature gates, plus FLASH0 dual-slot staging
with a retained journal, interrupted-update fallback, one-boot trials, explicit
confirmation, and rollback.

The active power/real-time fixtures add PWRCLK0 idle/deep sleep wake arming
with source filtering and low-frequency clock restoration, plus a deterministic
RTOS guard for fixed-priority releases, nominal deadlines, jitter limits, and
execution budgets.

The active `resilient-serial-service` fixture redirects GNU/Linux and POSIX
device, signal, polling, and termios calls into a deterministic script. Its
public tests cover raw 115200-8N1 configuration, bounded callback delivery,
async-signal-safe self-pipe shutdown, handler restoration, hot-unplug recovery,
transient versus fatal errors, and capped/resetting reconnect backoff.

The active `supervised-process-service` fixture redirects a supplied worker
launcher plus Linux/POSIX pidfd, sequence-preserving IPC, signal, polling,
termination, and reaping calls into a deterministic script. Its public tests
cover exact bounded request/ack records, at-least-once resend, exit and timeout
recovery, capped/resetting restart backoff, signal priority, protocol rejection,
zombie-free cleanup, and bounded SIGTERM-to-SIGKILL escalation.

The active `timer-dma-handoff` fixture uses opaque deterministic TIMER0/DMA0
models with explicit compare-stream ownership, terminal status latches, and
exact foreground interrupt-state restoration. Its public tests verify safe
descriptor handoff, completion ownership return, abort acknowledgement,
retained-compare recovery, error priority, and invalid-call boundaries.

The active `timer-capture-overflow` fixture uses an opaque deterministic TIMER1
model with a free-running 16-bit counter, capture/overflow/compare latches,
and exact foreground interrupt-state restoration. Its public tests verify
delayed-overflow timestamp reconstruction, bounded compare scheduling across a
wrap, stale-status acknowledgement, retained capture behavior, and overrun
accounting.

The current sandbox runner accepts active fixtures for the native-binary
profiles `c11-host`, `cpp17-host`, `go-std`, and `stable-rust`, the dependency-free
`python3-stdlib` interpreter profile, and the runtime-attested
`node-typescript`, `node-typescript-postgresql`, `python3-pytest-hypothesis`,
`postgresql`, and `react18-typescript` profiles. Other dependency-bearing and
service fixtures must remain scaffolds until their exact
packages and test runtimes can be verified, mounted, and executed in the test
namespace.

Fixture manifests use schema version 1.5; legacy 1.4 manifests remain
accepted. Public result records are emitted at version 1.5, with legacy 1.3
and 1.4 records still accepted. Validation reports are emitted at version
1.7, with legacy 1.6 reports accepted. Mutation catalogs use version 1.3.

Run `npm run fixtures:check` to validate task/profile references, manifests,
safe paths, tracked directory structure, and deterministic firmware host
coverage. This command validates fixture metadata and mock wiring only; it
does not execute compiler commands.

`mutations.json` follows `schemas/fixture-mutations.schema.json`. Every active
fixture supplies exact, single-match source substitutions derived from its
trusted reference. Catalogs may mutate the answer directly or hold a trusted
answer fixed while mutating a declared supplied starter/mock input.
`npm run test:mutations` stages each candidate and its
validator-owned inputs in a temporary directory, rewrites declared `build/`
artifacts when present, then runs the manifest's compile and test argv without
a shell. Each mutant must compile for that fixture's language and then fail
the public tests; compilation failures are invalid mutations, not successful
detections. Mutation tests never use extracted model output.

Extract one successful raw result with:

```bash
npm run fixture:extract -- --result results/<task-id>--<model-id>.json
```

The extractor unwraps provider output and applies the manifest's single-file or
bundle contract. Bundles require each declared heading, path, language, and
fence exactly once and in order, then stage all files before replacing the
ignored `generated/` directory. Extraction rejects failed, mismatched, or
stale-prompt results, malformed or oversized content, unsafe paths, and
existing output. Use `--force` only when intentionally replacing a previous
extraction. Extraction never compiles or executes model output.

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

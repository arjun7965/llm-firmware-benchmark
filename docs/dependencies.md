# Dependencies and Validation Toolchains

## Required to Run the Harness

The repository itself has no npm package dependencies. Running benchmarks,
tests, exports, and summaries requires:

- Node.js 22 or newer;
- either the NCode CLI or an OpenAI-compatible HTTP endpoint; and
- model access or credentials required by the selected provider.

Task-language toolchains are not required when the harness only collects model
responses. They are required when an evaluator compiles or executes generated
code. Each task references a pinned hosted runtime contract from
`validation-profiles.json`; see `docs/validation-profiles.md`.

## Optional Task Validation

| Task ID | Validation dependencies |
| --- | --- |
| `frontend-autocomplete` | Node.js 22.16.0, TypeScript 5.8.3, React/React DOM 18.3.1, jsdom 26.1.0, Testing Library 16.3.0, and pinned type declarations |
| `backend-idempotency` | Node.js 22.16.0, TypeScript 5.8.3, Express 5.1.0, `pg` 8.16.0, and PostgreSQL 16.9 |
| `bare-metal-timer` | A C11 compiler for host MMIO tests; `arm-none-eabi-gcc` is optional for Cortex-M3 compile-only validation |
| `interrupt-vector-configuration` | A C11 compiler plus fixture-owned vector-table, SCB, NVIC, barrier, linker-address, and interrupt-mask mocks; `arm-none-eabi-gcc` is optional for Cortex-M3 compile-only validation |
| `linker-memory-map` | A C11 compiler plus fixture-owned opaque linker-symbol, flash, and SRAM transfer mocks; `arm-none-eabi-gcc` is optional for Cortex-M3 compile-only validation |
| `i2c-controller-recovery` | A C11 compiler plus fixture-owned opaque I2C0, status, and protocol-ordering mocks; `arm-none-eabi-gcc` is optional for Cortex-M3 compile-only validation |
| `gpio-edge-debounce` | A C11 compiler plus fixture-owned opaque GPIO0 edge/wake latches and interrupt-mask mock; `arm-none-eabi-gcc` is optional for Cortex-M3 compile-only validation |
| `adc-threshold-watchdog` | A C11 compiler plus fixture-owned opaque ADC0 threshold/status and interrupt-mask mock; `arm-none-eabi-gcc` is optional for Cortex-M3 compile-only validation |
| `pwm-synchronized-update` | A C11 compiler plus fixture-owned opaque PWM0 shadow/load, status, and interrupt-mask mock; `arm-none-eabi-gcc` is optional for Cortex-M3 compile-only validation |
| `watchdog-window-recovery` | A C11 compiler plus fixture-owned opaque WDT0 counter/reset-cause/feed and interrupt-mask mock; `arm-none-eabi-gcc` is optional for Cortex-M3 compile-only validation |
| `timer-dma-handoff` | A C11 compiler plus fixture-owned opaque TIMER0/DMA0 ownership, compare-stream, status, and interrupt-mask mock; `arm-none-eabi-gcc` is optional for Cortex-M3 compile-only validation |
| `timer-capture-overflow` | A C11 compiler plus fixture-owned opaque TIMER1 capture/compare, overflow-status, and interrupt-mask mock; `arm-none-eabi-gcc` is optional for Cortex-M3 compile-only validation |
| `uart-interrupt-driver` | A C11 compiler plus the fixture-owned UART0 MMIO and interrupt-mask mock; `arm-none-eabi-gcc` is optional for Cortex-M3 compile-only validation |
| `spi-dma-transfer` | A C11 compiler plus fixture-owned opaque SPI0/DMA0 registers, DMA buffer-address translation, and an interrupt-mask mock; `arm-none-eabi-gcc` is optional for Cortex-M3 compile-only validation |
| `can-controller-recovery` | A C11 compiler plus fixture-owned opaque CAN0 mailbox/status and interrupt-mask mocks; `arm-none-eabi-gcc` is optional for Cortex-M3 compile-only validation |
| `interrupt-deferred-work` | A C11 compiler plus fixture-owned opaque nested-priority interrupt-latch and interrupt-mask mocks; `arm-none-eabi-gcc` is optional for Cortex-M3 compile-only validation |
| `embedded-ring-buffer` | A C11 compiler with `<stdatomic.h>` support, such as GCC or Clang |
| `static-memory-pool` | A C11 compiler with `<stdalign.h>` and `<stdint.h>` support; no runtime mock or allocator dependency |
| `fixed-point-stack-budget` | A C11 compiler with fixed-width integer support and `-Wvla` diagnostics; no runtime mock or floating-point dependency |
| `dma-cache-coherency` | A C11 compiler plus fixture-owned cache-maintenance/DMA ordering mock; `arm-none-eabi-gcc` is optional for Cortex-M7-compatible ARMv7-M compile-only validation |
| `firmware-state-machine` | A C11 compiler plus a deterministic mock implementation of the supplied HAL |
| `rtos-priority-inversion` | A C11 compiler plus the fixture-owned deterministic RTOS priority-inheritance mock |
| `rtos-periodic-scheduler` | A C11 compiler plus the fixture-owned deterministic RTOS release/deadline mock |
| `rtos-queue-semaphore` | A C11 compiler plus fixture-owned fixed-capacity RTOS queue and counting-semaphore mocks |
| `rtos-event-flags-deadlock` | A C11 compiler plus fixture-owned RTOS event-flag and ordered-mutex mocks |
| `binary-parser` | A C11 compiler; sanitizers are recommended for executable tests |
| `modbus-rtu-receiver` | A C11 compiler with fixed-width integer support; no runtime mock or serial dependency |
| `can-transport-reassembly` | A C11 compiler with fixed-width integer support; no CAN controller, transport stack, or runtime mock dependency |
| `ble-advertising-reassembly` | A C11 compiler with fixed-width integer support; no BLE radio, stack, or runtime mock dependency |
| `concurrency-debug` | Python 3.12.11 using only its standard library |
| `postgres-pagination` | PostgreSQL 16.9 `initdb`, `pg_ctl`, `postgres`, and `psql` |
| `testing-property-based` | Python 3.12.11, pytest 8.4.0, Hypothesis 6.135.9, and the hash-pinned pure-Python transitive closure |
| `go-graceful-shutdown` | Go 1.24.4; only the standard library is used |
| `rust-stream-decoder` | Rust/Cargo 1.87.0 and GCC 13.3.0 as the system linker; standard library only |
| `typescript-singleflight-cache` | Node.js 22.16.0, TypeScript 5.8.3, and `@types/node` 22.15.29 |
| `webhook-replay-security` | Node.js 22.16.0, TypeScript 5.8.3, Express 5.1.0, `pg` 8.16.0, and PostgreSQL 16.9 |

These tools are optional because automated answer extraction and compilation
are not yet part of the harness. The profile registry separates logical
requirements from concrete execution environments. Together they fix tool
versions, package versions, sandbox policy, and resource limits. Validation
reports record both contract fingerprints, resolved versions, compile flags,
and commands. Use the same exact profile and environment IDs, revisions, and
SHA-256 values for all models in a deterministic comparison. The repeat-score
contract pins one compatible profile/environment pair per deterministic scored
task and verifies the profile ID against the task registry. Tasks explicitly
classified as rubric-only do not receive a validation contract; see
`docs/rubric-only-tasks.md`.
Profiles with npm or PyPI dependencies additionally pin committed lockfiles
under `validation-locks/`; startup verifies their SHA-256 and package set.
Those lockfiles are stored with LF line endings and normalized before hashing
so Git checkout settings do not change the attested contract.
The current sandbox runner verifies and mounts the `node-typescript`,
`node-typescript-postgresql`, `python3-pytest-hypothesis`, and
`react18-typescript` installations. Other dependency-bearing profiles remain
rejected until they define equivalent installed-tree attestation or run in a
digest-pinned image.
Dependency-free interpreter and service profiles may declare profile-approved
test-runtime mounts and command prefixes. The runner supports the pinned
`python3-stdlib` runtime and a PostgreSQL lifecycle that creates a fresh data
directory and private Unix socket per phase. The Node/PostgreSQL profile mounts
its attested package tree read-only in the candidate namespace while the
database server remains separate.

Keep validator-only packages outside the root project or in a future isolated
fixture directory. Do not add runtime dependencies to this dependency-free
harness solely to score one task.

Embedded task toolchains and runtime assumptions are defined separately in
`docs/embedded/target-assumptions.md`.

Fixture manifests declare a matching validation profile, required tools, and
commands. `npm run fixtures:check` validates those declarations but does not
install tools or execute generated code.

Sandboxed model-answer validation is Linux-only and additionally requires:

- Bubblewrap (`bwrap`);
- `prlimit` from util-linux; and
- the toolchain declared by the selected fixture.

These executables must resolve to root-owned, non-writable files under `/usr`.
Ubuntu 24.04 also requires an AppArmor policy that permits Bubblewrap to create
its user namespace. CI installs `apparmor-profiles` and enables the packaged
`bwrap-userns-restrict` policy; it does not disable AppArmor or run the
validator as root.
Run `npm run fixture:validate -- --task <task-id>`. The command fails closed if
isolation is unavailable. `npm run test:sandbox` validates the sandbox runner
against trusted references only. Validation reports automatically capture the
resolved toolchain version using each manifest's fixed `toolVersionArgs`,
compile/test argv, suite and target metadata, native artifact size when
applicable, outcomes, and diagnostics.
See `docs/sandboxed-validation.md`.

`npm run test:mutations` uses each active fixture's declared compile and test
commands to build controlled mutations of trusted references. Every mutation
must be compile-valid for that fixture's language and be rejected by the public
tests. `npm run test:c` includes these mutation checks for the current C
fixtures.

The fictional bare-metal timer fixture uses `cc` for instrumented host MMIO:

```bash
npm run fixture:timer:self-test
```

The interrupt-vector configuration fixture uses `cc` with deterministic RAM
table, SCB/NVIC, linker-address, barrier, and interrupt-mask models:

```bash
npm run fixture:interrupt-vector:self-test
```

The linker-memory-map fixture uses `cc` with opaque linker-symbol, flash, and
SRAM models to verify layout validation, initialized-data copying, BSS clearing,
and address-boundary queries:

```bash
npm run fixture:linker-memory:self-test
```

The I2C-controller recovery fixture uses `cc` with an opaque I2C0 model that
records status, data, and control ordering across arbitration-loss and timeout
recovery:

```bash
npm run fixture:i2c:self-test
```

The GPIO edge/debounce fixture uses `cc` with opaque GPIO0 edge/wake latches,
an active-low button model, and deterministic interrupt-state recording:

```bash
npm run fixture:gpio-debounce:self-test
```

The ADC threshold/watchdog fixture uses `cc` with opaque ADC0 status/data
latches, threshold registers, and deterministic interrupt-state recording:

```bash
npm run fixture:adc-watchdog:self-test
```

The PWM synchronized-update fixture uses `cc` with opaque PWM0 shadow/active
registers, period-boundary application, fault latches, and deterministic
interrupt-state recording:

```bash
npm run fixture:pwm:self-test
```

The watchdog-window recovery fixture uses `cc` with opaque WDT0 counter,
reset-cause, feed-window, and interrupt-mask recording:

```bash
npm run fixture:watchdog-window:self-test
```

The timer-DMA ownership handoff fixture uses `cc` with opaque TIMER0/DMA0
register models, deterministic compare-stream boundaries, and interrupt-mask
recording:

```bash
npm run fixture:timer-dma:self-test
```

The timer capture/compare overflow fixture uses `cc` with opaque TIMER1
counter/capture/compare status, deterministic delayed-wrap stimuli, and
interrupt-mask recording:

```bash
npm run fixture:timer-capture:self-test
```

The UART interrupt-driver fixture uses `cc` with an instrumented UART0 register
bank and deterministic interrupt-mask model:

```bash
npm run fixture:uart:self-test
```

The SPI DMA transfer fixture uses `cc` with fixture-owned opaque SPI0/DMA0
register models, deterministic full-duplex data movement, and an
interrupt-mask model:

```bash
npm run fixture:spi-dma:self-test
```

The CAN-controller recovery fixture uses `cc` with fixture-owned opaque CAN0
mailbox, terminal-status, bus-off, and interrupt-mask models:

```bash
npm run fixture:can:self-test
```

The interrupt deferred-work fixture uses `cc` with fixture-owned opaque
two-priority latch, deterministic nested-IRQ injection, C11 atomics, and an
exact interrupt-mask model:

```bash
npm run fixture:interrupt-work:self-test
```

The trusted firmware fixture self-test requires `cc`:

```bash
npm run fixture:firmware:self-test
```

The RTOS priority-inversion fixture uses the same C11 compiler and a
fixture-owned scheduler mock:

```bash
npm run fixture:rtos:self-test
```

The RTOS periodic scheduler, queue/semaphore, and event-flag/deadlock fixtures
use the same C11 compiler with fixture-owned deterministic RTOS boundaries:

```bash
npm run fixture:rtos-scheduler:self-test
npm run fixture:rtos-queue:self-test
npm run fixture:rtos-events:self-test
```

The trusted ring-buffer fixture uses the same C11 compiler:

```bash
npm run fixture:ring-buffer:self-test
```

The constrained-memory fixtures use the same C11 compiler; the DMA
cache-coherency fixture also uses its fixture-owned deterministic cache/DMA
ordering mock:

```bash
npm run fixture:static-memory-pool:self-test
npm run fixture:dma-cache:self-test
npm run fixture:fixed-point:self-test
```

The trusted binary-parser fixture also requires `cc`:

```bash
npm run fixture:parser:self-test
```

The active Rust stream-decoder fixture requires Rust/Cargo 1.87.0 and its
GCC 13.3.0 linker. Its calibration command compiles the reference and all
controlled mutations, including initialization-time successful exit; sandbox
CI verifies the same pinned toolchain set:

```bash
npm run fixture:rust-decoder:self-test
```

The active concurrency-debug fixture requires the root-owned Python 3.12.11
runtime mounted read-only in the test namespace. Its calibration runs the
reference and all controlled mutations:

```bash
npm run fixture:concurrency:self-test
```

The active TypeScript singleflight-cache fixture requires the attested
`node-typescript` revision 4 installation. Its calibration command verifies the
trusted reference and all controlled mutations:

```bash
npm run fixture:typescript-cache:self-test
```

The active backend idempotency fixture requires the attested
`node-typescript-postgresql` revision 5 package tree, pinned Node.js 22.16.0,
and the root-owned PostgreSQL 16.9 runtime. Its calibration runs a fresh
database for every compile and test phase and rejects eleven controlled
idempotency, input-validation, and atomicity defects:

```bash
npm run fixture:backend-idempotency:self-test
```

The active webhook replay security fixture uses the same attested
`node-typescript-postgresql` revision 5 package tree and PostgreSQL runtime.
Its calibration rejects eleven raw-body authentication, secret-rotation,
replay, outbox, and transaction-atomicity defects:

```bash
npm run fixture:webhook-replay-security:self-test
```

The active PostgreSQL pagination fixture requires the pinned root-owned
PostgreSQL 16.9 installation. CI builds the official source archive after
checking its committed SHA-256, then calibrates the reference and twelve
controlled defects:

```bash
npm run fixture:postgres-pagination:self-test
```

The active property-based testing fixture requires the attested
`python3-pytest-hypothesis` revision 4 package tree. Its calibration keeps the
trusted test answer fixed and requires all controlled `pathutil` defects to be
detected:

```bash
npm run fixture:property-tests:self-test
```

The active frontend autocomplete fixture requires the attested
`react18-typescript` revision 4 tree. Its fake-timer/jsdom suite exercises the
trusted component and rejects all controlled async and accessibility defects:

```bash
npm run fixture:frontend-autocomplete:self-test
```

Run all trusted C fixture suites with:

```bash
npm run test:c
```

## Optional Cross-Compilation

Representative bare-metal checks use:

- `arm-none-eabi-gcc` from `gcc-arm-none-eabi` for Cortex-M3/Thumb;
- `riscv64-unknown-elf-gcc` from `gcc-riscv64-unknown-elf` with
  `-march=rv32imac -mabi=ilp32`.

Run `npm run cross:check`; missing tools are reported and skipped. CI or strict
local validation should use `npm run cross:check -- --require-tools`. Select one
profile with `--target armv7m-bare-metal` or `--target rv32-bare-metal`.
Cross-compilation creates temporary object files from trusted references only;
it does not link or execute target or model-generated code.

Useful version checks include:

```bash
node --version
cc --version
arm-none-eabi-gcc --version
riscv64-unknown-elf-gcc --version
python3 --version
go version
rustc --version
cargo --version
psql --version
```

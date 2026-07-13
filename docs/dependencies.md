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
| `frontend-autocomplete` | Node.js, TypeScript, React 18 and its type declarations; a browser-like test environment for interaction tests |
| `backend-idempotency` | Node.js, TypeScript, Express, `pg`, and PostgreSQL |
| `bare-metal-timer` | A C11 compiler for host MMIO tests; `arm-none-eabi-gcc` is optional for Cortex-M3 compile-only validation |
| `embedded-ring-buffer` | A C11 compiler with `<stdatomic.h>` support, such as GCC or Clang |
| `firmware-state-machine` | A C11 compiler plus a deterministic mock implementation of the supplied HAL |
| `binary-parser` | A C11 compiler; sanitizers are recommended for executable tests |
| `concurrency-debug` | Python 3.12.11 using only its standard library |
| `postgres-pagination` | PostgreSQL server and client tools for schema, query, and `EXPLAIN` validation |
| `testing-property-based` | Python 3.12.11, pytest 8.4.0, Hypothesis 6.135.9, and the hash-pinned pure-Python transitive closure |
| `go-graceful-shutdown` | Go 1.24.4; only the standard library is used |
| `rust-stream-decoder` | Rust/Cargo 1.87.0 and GCC 13.3.0 as the system linker; standard library only |
| `typescript-singleflight-cache` | Node.js 22.16.0, TypeScript 5.8.3, and `@types/node` 22.15.29 |
| `webhook-replay-security` | Node.js, TypeScript, Express, `pg`, and PostgreSQL |

These tools are optional because automated answer extraction and compilation
are not yet part of the harness. The profile registry separates logical
requirements from concrete execution environments. Together they fix tool
versions, package versions, sandbox policy, and resource limits. Validation
reports record both contract fingerprints, resolved versions, compile flags,
and commands. Use the same exact profile and environment IDs, revisions, and
SHA-256 values for all models in a comparison. The repeat-score contract pins
one compatible profile/environment pair per scored task and verifies the
profile ID against the task registry.
Profiles with npm or PyPI dependencies additionally pin committed lockfiles
under `validation-locks/`; startup verifies their SHA-256 and package set.
Those lockfiles are stored with LF line endings and normalized before hashing
so Git checkout settings do not change the attested contract.
The current sandbox runner verifies and mounts the `node-typescript` and
`python3-pytest-hypothesis` installations. Other dependency-bearing profiles
remain rejected until they define equivalent installed-tree attestation or run
in a digest-pinned image.
Dependency-free interpreter and service profiles may declare profile-approved
test-runtime mounts and command prefixes. The runner supports the pinned
`python3-stdlib` runtime; service runtimes remain scaffold-only until their
complete execution boundary is implemented.

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

The trusted firmware fixture self-test requires `cc`:

```bash
npm run fixture:firmware:self-test
```

The trusted ring-buffer fixture uses the same C11 compiler:

```bash
npm run fixture:ring-buffer:self-test
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

The active property-based testing fixture requires the attested
`python3-pytest-hypothesis` revision 4 package tree. Its calibration keeps the
trusted test answer fixed and requires all controlled `pathutil` defects to be
detected:

```bash
npm run fixture:property-tests:self-test
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

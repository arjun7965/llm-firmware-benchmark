# Validation Profiles

`validationProfile` identifies the hosted runtime and toolchain used to validate
an answer. It is required for every task and is separate from `targetProfile`,
which records architecture, ABI, and hardware assumptions. Neither field adds
hidden text to a model prompt.

A profile does not by itself authorize executable scoring. `tasks.json` also
declares `scoringMode`: only `deterministic` tasks may use a fixture and record
a profile/environment validation contract. `rubric-only` tasks retain their
profile as task context but are scored through their published rubric under the
[rubric-only task policy](rubric-only-tasks.md).

The machine-readable contracts are pinned in `validation-profiles.json` and
validated against `schemas/validation-profiles.schema.json`.
`src/validation-profiles.mjs` loads and enforces the registry. Tasks, fixture
manifests, raw results, and public exports preserve the logical profile ID.
Validation reports additionally preserve the selected concrete environment.

A logical profile defines required tool names, package dependencies, sandbox
policy, tmpfs sizes, compile/test resource limits, and exact supported
environment references. A concrete environment independently defines its OS
release, architecture, execution mode, exact toolchain versions, and exact
sandbox runtime versions. Host environments pin Bubblewrap and `prlimit`; OCI
environments pin Podman. This lets one logical profile support
multiple distributions, architectures, or image revisions without treating
those hosts as the same scoring environment.

Both contract types have immutable, contiguous revisions. Append a new
environment revision when a host, image digest, or environment-specific tool
pin changes. Append a new profile revision when its logical requirements or
supported environment references change. Never replace a published revision.
Tasks and new validations use the highest profile revision, while report
validation resolves the exact profile and environment revisions recorded by
the report.

The append-only `validation-profile-fingerprints.json` registry records the
canonical SHA-256 for every published profile and environment revision.
Startup fails if a contract is changed, removed, or added without its matching
fingerprint. Legacy revision 1 profiles remain available for historical
fingerprint verification; revision 2 profiles use the separated environment
model. Dependency-bearing profiles introduced lockfile attestation in revision
3. Dependency-free interpreter and service
profiles introduced command contracts in revision 3; `python3-stdlib`
revision 4 pins the relocatable runtime tree mounted by the runner, and
`node-typescript` revision 4 adds installed-tree attestation and runtime mounts.
`go-std` revision 4 raises its bounded temporary filesystem to 256 MiB so Go
1.24.4 can compile the standard library inside the sandbox; revision 5 raises
the compile-phase file-descriptor limit from 128 to 256 for that toolchain.
`c11-host` revision 3 adds a separately pinned Debian 13 x86-64 environment
while retaining the existing Ubuntu 24.04 environment.
`react18-typescript` revision 4 pins the complete npm package-lock and installed
tree, mounts that tree for both TypeScript compilation and jsdom tests, and
provides each Node.js phase a 2 GiB virtual-address limit.

## Profile Registry

Current profiles reference profile-scoped Ubuntu 24.04 x86-64 host
environments. `c11-host` additionally supports a Debian 13 x86-64 host
environment. Every environment pins only the toolchains required by that
profile, plus host execution and its exact Bubblewrap and `prlimit` versions.
Fixture manifests and reports must cover that complete toolchain set, so the
environment fingerprint never attests an unprobed tool. Profile policies
require no network and an isolated filesystem.

| Profile | Pinned toolchains | Pinned packages | Test command contract |
| --- | --- | --- | --- |
| `c11-host` | Ubuntu: GCC/`cc` 13.3.0; Debian: GCC/`cc` 14.2.0 | None | Native `build/` executable |
| `cpp17-host` | GCC/`cc` and `c++` 13.3.0 | None | C11 mock object linked with a C++17 native `build/` executable |
| `go-std` | Go 1.24.4 | None; standard library only | Native `build/` executable |
| `node-typescript` | Node.js 22.16.0, TypeScript 5.8.3 | TypeScript and Node.js types | `tsc` compile and Node.js public-test commands |
| `node-typescript-postgresql` | Node.js 22.16.0, PostgreSQL 16.9 | Express 5.1.0, `pg` 8.16.0, TypeScript 5.8.3, and types | Validator-owned TypeScript compile launcher plus Node.js public tests over a fresh private PostgreSQL socket |
| `postgresql` | PostgreSQL 16.9 `initdb`, `pg_ctl`, server, and client | None | Fresh isolated cluster plus approved `psql -X -v ON_ERROR_STOP=1 ...` commands |
| `python3-pytest-hypothesis` | Python 3.12.11, pytest 8.4.0 | Hash-pinned pytest, Hypothesis, and pure-Python transitive closure | Pytest property-test modules with deterministic Hypothesis settings |
| `python3-stdlib` | Python 3.12.11 | None; standard library only | `python3 -m py_compile ...` and `python3 -m unittest ...` with Python runtime mounts |
| `react18-typescript` | Node.js 22.16.0, TypeScript 5.8.3 | React 18.3.1, jsdom 26.1.0, Testing Library, and exact type declarations | `tsc` compile and Node.js jsdom interaction tests |
| `stable-rust` | Rust/Cargo 1.87.0 and GCC/`cc` 13.3.0 | None; standard library only | Native `build/` executable |

The registry is authoritative for full dependency versions and byte-level
resource limits. Dependency-bearing current profiles also record a
`dependencyInstall` attestation. Committed attestations live under
`validation-locks/`; each profile pins the lockfile path, source (`npm` or
`pypi`), and SHA-256. Startup verifies the file hash and direct package set.
The runtime-enabled `node-typescript` profile additionally uses an npm
package-lock with registry integrity values for the complete transitive
closure and pins the canonical hash of its installed tree.
The runtime-enabled `node-typescript-postgresql` profile applies the same
attestation to Express, `pg`, TypeScript, and their declarations. It mounts the
Node runtime and package tree read-only for the candidate and runs PostgreSQL
in a separate no-network namespace joined only by a fresh Unix socket.
The runtime-enabled `react18-typescript` profile applies the same attestation
to its React, jsdom, Testing Library, TypeScript, and declaration package tree;
the runner mounts it read-only during both compile and test phases.
The runtime-enabled `python3-pytest-hypothesis` revision 4 profile uses a
hash-checking pip requirements lock and pins its complete installed tree. The
runner mounts that tree, the pytest launcher, and the Python runtime read-only;
Hypothesis state is redirected to the sandbox's private temporary filesystem.
Git attributes force those lockfiles to LF line endings, and the verifier
normalizes CRLF to LF before hashing so platform checkout settings do not
change the contract.
Fixture manifests may invoke only toolchains declared by their profile and
must use version-probe argv supported by every referenced environment. Before
probing tools, the sandbox validator reads `/etc/os-release` and normalizes the
runtime architecture. Without an explicit selection, it chooses exactly one
matching host environment; zero or multiple matches fail closed. OCI
environments must be selected by exact ID, run only on Linux with the matching
architecture, and never take precedence over a host environment implicitly.
The validator rejects any resolved tool or sandbox runtime version that differs
from that environment's pins.

Reports record the profile and environment IDs, revisions, contract SHA-256
values, detected host fields, and execution mode. `repeat-scores.json` requires
the same immutable profile/environment pair per scored task, outside the
model/run records. The summarizer verifies the profile against `tasks.json`
and verifies that the environment revision is supported by that exact profile
revision. Consequently, all models and runs compared for a task must use the
same contracts while unrelated tasks may use different environments.
Evaluators must check that every validation report used as scoring evidence
matches that task's declared pair.

## OCI Execution

Digest-pinned OCI images are supported as a portable alternative for profiles
whose complete userspace should not depend on the validator's distribution.
The registry includes `debian-12-x86-64-c11-oci@1` for `c11-host@4`. Its
reviewed recipe, Linux/amd64 platform-manifest digest, source revision, runtime
security contract, and CI calibration are committed under `oci/c11/`. Future
environments are activated only after the same evidence is reviewed. Activation
appends an environment and, when necessary, a logical-profile revision. An OCI
environment is never selected implicitly. Invoke the registered C11 one with:

```bash
npm run fixture:validate -- --task <task-id> \
  --environment debian-12-x86-64-c11-oci
```

The execution contract accepts only `image@sha256:<platform-manifest-digest>`
references and records the reviewed GitHub source repository and 40-character
source revision. Podman first inspects the already-local image without pulling,
requires the observed digest and Linux architecture to match, and verifies the
`org.opencontainers.image.source` and
`org.opencontainers.image.revision` labels. Toolchain probes then run inside
the same hardened image contract. Reports preserve the image reference,
digest, local content ID, platform, source, revision, and rootless status.

OCI validation requires a non-root caller and the environment's exact Podman
version. Each environment also pins an exact generated `containers.conf`
SHA-256, `/usr/bin/crun` or `/usr/bin/runc`, `/usr/bin/conmon`, their versions,
and the SHA-256 of `/usr/share/containers/seccomp.json`. The runner places the
configuration and an empty `XDG_CONFIG_HOME` in a private state directory and
passes an allowlisted host environment to every Podman process, including
service-supervisor children. Runtime inspection must prove that Podman is
local and rootless, uses the registered low-level runtime, monitor, seccomp
profile, cgroup v2 with `systemd`, and the non-persistent `none` event and log
backends. Each invocation disables pulls and networking, clears inherited and
image-defined environment variables, ignores image-declared volumes, drops all
capabilities, sets `no-new-privileges`, disables automatic systemd behavior,
and uses a read-only image root. The reviewed image must define UID/GID 65532;
rootless `keep-id` mapping maps the host caller to that unprivileged identity.
Only bounded `/tmp` and `/run` tmpfs mounts and explicit fixture mounts are
writable.
The validator copies the starter, mocks, public tests, and extracted answer to
a private staging tree rather than exposing the repository. That tree is
read-only; a separate build mount is writable only for compilation and becomes
read-only for testing. Memory, swap, process-count, address-space, CPU,
file-size, open-file, core-dump, wall-time, and captured-output limits apply.
Recorded container IDs are force-removed after errors or supervisor teardown.
The CID file, runtime configuration, and service state remain on disk with a
recovery path in the error if forced removal fails; successful cleanup removes
them.

An image is eligible for registration only after review establishes all of the
following:

- every base image uses a digest and every downloaded artifact or package
  closure is checksum- or lockfile-pinned;
- the build recipe, lockfiles, source revision, target architecture, and
  UID/GID 65532 account are committed and reviewable;
- rebuilding the same source revision produces the reviewed filesystem content
  under the project's documented build procedure;
- trusted reference and mutation calibration pass inside that exact platform
  image before its digest is added to the registry; and
- the published image carries the required source and revision labels, and its
  platform-manifest digest is independently resolved before registration;
- the Podman, low-level runtime, monitor, generated configuration, and seccomp
  fingerprints are captured on the same runner image used by calibration; and
- CI exercises trusted references and the mutation catalog through that exact
  registered OCI environment.

Preloading or publishing images is a trusted provisioning step outside model
answer validation. The runner never authenticates to a registry or falls back
to a tag or a different local image. Dependency-bearing OCI-only profile
revisions use `dependencyInstall.kind: "oci-image"` with the same digest as
every referenced environment. Contract revisions and fingerprints remain
append-only, so changing a recipe, package, tool, runtime, or image produces a
new environment revision and digest.

The host Bubblewrap runner supports `node-typescript` by hashing its complete
root-owned installed tree and comparing it with the profile contract before
mounting it read-only. The npm package-lock pins the complete package closure
and registry integrity values; the installed-tree hash additionally pins file
contents, modes, paths, and directory layout. Dependency profiles without
these runtime fields continue to fail closed before tools are resolved or
executed. Standard-library-only profiles are not automatically eligible: the
test sandbox supports native binaries produced by `c11-host`, `cpp17-host`, `go-std`, and
`stable-rust`, plus approved interpreter commands for `python3-stdlib`,
`python3-pytest-hypothesis`, `node-typescript`, `postgresql`, and
`node-typescript-postgresql`, and `react18-typescript`. PostgreSQL-backed
profiles additionally record fixed
initialize, start, readiness, and stop argv. Its server and candidate client
run in separate no-network Bubblewrap namespaces joined only by a fresh
temporary Unix socket directory. Readiness also creates a non-superuser
database-owner role used for all candidate SQL, then disables login on the
bootstrap superuser so candidate scripts cannot reconnect with another role.

## Task Mapping

| Tasks | Validation profile |
| --- | --- |
| `adc-threshold-watchdog`, `bare-metal-timer`, `binary-parser`, `ble-advertising-reassembly`, `brownout-safe-mode`, `can-controller-recovery`, `can-transport-reassembly`, `compiler-trace-regression-debug`, `cortex-m-hard-fault-debug`, `dma-cache-coherency`, `embedded-ring-buffer`, `fault-crash-record`, `firmware-state-machine`, `fixed-point-filter-optimization`, `fixed-point-stack-budget`, `gpio-edge-debounce`, `i2c-controller-recovery`, `idempotent-system-init`, `interrupt-deferred-work`, `interrupt-vector-configuration`, `linker-memory-map`, `modbus-rtu-receiver`, `pwm-synchronized-update`, `resilient-serial-service`, `rtos-event-flags-deadlock`, `rtos-periodic-scheduler`, `rtos-priority-inversion`, `rtos-queue-semaphore`, `spi-dma-transfer`, `static-memory-pool`, `supervised-process-service`, `timer-capture-overflow`, `timer-dma-handoff`, `uart-interrupt-driver`, `watchdog-window-recovery` | `c11-host` |
| `secure-boot-image-validation`, `secure-maintenance-command`, `mpu-fault-containment`, `dual-slot-update-recovery` | `c11-host` |
| `low-power-wake-clock`, `real-time-deadline-budget` | `c11-host` |
| `mixed-c-cpp-mmio-safety-review` | `cpp17-host` |
| `frontend-autocomplete` | `react18-typescript` |
| `backend-idempotency`, `webhook-replay-security` | `node-typescript-postgresql` |
| `concurrency-debug` | `python3-stdlib` |
| `postgres-pagination` | `postgresql` |
| `testing-property-based` | `python3-pytest-hypothesis` |
| `go-graceful-shutdown` | `go-std` |
| `rust-stream-decoder` | `stable-rust` |
| `typescript-singleflight-cache` | `node-typescript` |

When adding a task, select a registered profile and document its dependencies.
Add a new profile only when its runtime, dependency, or isolation assumptions
cannot be represented by an existing profile. Validator-only dependencies
belong in fixture-local lock files or isolated images, not the root package.

# Validation Profiles

`validationProfile` identifies the hosted runtime and toolchain used to validate
an answer. It is required for every task and is separate from `targetProfile`,
which records architecture, ABI, and hardware assumptions. Neither field adds
hidden text to a model prompt.

The machine-readable contracts are pinned in `validation-profiles.json` and
validated against `schemas/validation-profiles.schema.json`.
`src/validation-profiles.mjs` loads and enforces the registry. Tasks, fixture
manifests, raw results, and public exports preserve the logical profile ID.
Validation reports additionally preserve the selected concrete environment.

A logical profile defines required tool names, package dependencies, sandbox
policy, tmpfs sizes, compile/test resource limits, and exact supported
environment references. A concrete environment independently defines its OS
release, architecture, execution mode, exact toolchain versions, and exact
Bubblewrap and `prlimit` versions. This lets one logical profile support
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
1.24.4 can compile the standard library inside the sandbox.
`react18-typescript` revision 4 pins the complete npm package-lock and installed
tree, mounts that tree for both TypeScript compilation and jsdom tests, and
provides each Node.js phase a 2 GiB virtual-address limit.

## Profile Registry

Each current profile references a profile-scoped Ubuntu 24.04 x86-64 host
environment. Every environment pins only the toolchains required by that
profile, plus host execution, Bubblewrap 0.9.0, and `prlimit` from util-linux
2.39.3. Fixture manifests and reports must cover that complete toolchain set,
so the environment fingerprint never attests an unprobed tool. Profile
policies require no network and an isolated filesystem.

| Profile | Pinned toolchains | Pinned packages | Test command contract |
| --- | --- | --- | --- |
| `c11-host` | GCC/`cc` 13.3.0 | None | Native `build/` executable |
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
probing tools, the sandbox validator reads `/etc/os-release`, normalizes the
runtime architecture, and selects exactly one supported host environment. Zero
or multiple matches fail closed. It then rejects any resolved tool, Bubblewrap,
or `prlimit` version that differs from that environment's pins.

Reports record the profile and environment IDs, revisions, contract SHA-256
values, detected host fields, and execution mode. `repeat-scores.json` requires
the same immutable profile/environment pair per scored task, outside the
model/run records. The summarizer verifies the profile against `tasks.json`
and verifies that the environment revision is supported by that exact profile
revision. Consequently, all models and runs compared for a task must use the
same contracts while unrelated tasks may use different environments.
Evaluators must check that every validation report used as scoring evidence
matches that task's declared pair.

## OCI Image Decision

Digest-pinned OCI images are the preferred future execution mode for profiles
with npm, PyPI, interpreter, or service dependencies. A digest can fix the
complete userspace and package installation across host distributions, which
host version probes cannot attest. The registry and schemas accept only
`image@sha256:<digest>` references for OCI environments.

The current runner intentionally implements only `host` environments. It
bind-mounts host toolchains and runtime libraries into Bubblewrap and has no
OCI runtime dependency or verified image-build pipeline. Treating an image tag
as reproducible, or nesting an unverified container invocation inside the
existing sandbox, would weaken the fail-closed boundary. Before activating an
OCI environment, add a rootless runtime contract, a reviewed build recipe and
lockfiles, image provenance, digest verification before execution, and tests
for the resulting mount and network boundary. An OCI-only environment is not
selected by the host runner.

The host Bubblewrap runner supports `node-typescript` by hashing its complete
root-owned installed tree and comparing it with the profile contract before
mounting it read-only. The npm package-lock pins the complete package closure
and registry integrity values; the installed-tree hash additionally pins file
contents, modes, paths, and directory layout. Dependency profiles without
these runtime fields continue to fail closed before tools are resolved or
executed. Standard-library-only profiles are not automatically eligible: the
test sandbox supports native binaries produced by `c11-host`, `go-std`, and
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
| `bare-metal-timer`, `binary-parser`, `embedded-ring-buffer`, `firmware-state-machine` | `c11-host` |
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

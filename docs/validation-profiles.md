# Validation Profiles

`validationProfile` identifies the hosted runtime and toolchain used to validate
an answer. It is required for every task and is separate from `targetProfile`,
which records architecture, ABI, and hardware assumptions. Neither field adds
hidden text to a model prompt.

The machine-readable contracts are pinned in `validation-profiles.json` and
validated against `schemas/validation-profiles.schema.json`.
`src/validation-profiles.mjs` loads and enforces the registry. Tasks, fixture
manifests, raw results, public exports, and validation reports preserve the
profile ID.

Every contract has an immutable revision, Ubuntu release and architecture,
exact toolchain and package versions, sandbox versions and policy, tmpfs sizes,
and compile/test resource limits. Validation reports record the revision,
contract SHA-256, resolved tool versions, and applied resource limits. Append a
new entry with the same profile ID and next contiguous revision whenever any
pinned value changes; do not replace a published revision. Tasks and new
validations use the highest revision, while report validation resolves the
exact ID and revision recorded by the report. The append-only
`validation-profile-fingerprints.json` registry records the canonical SHA-256
for every published ID and revision; startup fails if a contract is changed,
removed, or added without its corresponding fingerprint.

## Profile Registry

All revision 1 profiles use Ubuntu 24.04 x86-64, Bubblewrap 0.9.0, `prlimit`
from util-linux 2.39.3, no network, and an isolated filesystem.

| Profile | Pinned toolchains | Pinned packages |
| --- | --- | --- |
| `c11-host` | GCC/`cc` 13.3.0 | None |
| `go-std` | Go 1.24.4 | None; standard library only |
| `node-typescript` | Node.js 22.16.0, TypeScript 5.8.3 | TypeScript and Node.js types |
| `node-typescript-postgresql` | Node.js 22.16.0, TypeScript 5.8.3, PostgreSQL 16.9 | Express, `pg`, TypeScript, and types |
| `postgresql` | PostgreSQL server and client 16.9 | None |
| `python3-pytest-hypothesis` | Python 3.12.11, pytest 8.4.0 | pytest 8.4.0, Hypothesis 6.135.9 |
| `python3-stdlib` | Python 3.12.11 | None; standard library only |
| `react18-typescript` | Node.js 22.16.0, TypeScript 5.8.3 | React 18.3.1 and the exact test stack in the registry |
| `stable-rust` | Rust and Cargo 1.87.0 | None; standard library only |

The registry is authoritative for full dependency versions and byte-level
resource limits. Fixture manifests may invoke only toolchains declared by
their profile and must use the profile's version-probe argv. The sandbox
validator fails when resolved tool, Bubblewrap, or `prlimit` versions do not
match the pins. Before probing tools, it reads `/etc/os-release`, normalizes the
runtime architecture, and rejects a host that does not exactly match the
selected revision.

The current Bubblewrap runner cannot prove installed npm or PyPI package
versions. It therefore fails closed for every profile with a nonempty
`dependencies` array before resolving or executing tools. Such a profile can
be activated only after its fixture provides a verified lockfile installation
or validation runs in a digest-pinned image. Standard-library-only profiles
are not automatically eligible: the current test sandbox supports only native
binaries produced by `c11-host`, `go-std`, and `stable-rust`.
Interpreter and service profiles remain scaffold-only until their approved
runtime is explicitly mounted in the test namespace.

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

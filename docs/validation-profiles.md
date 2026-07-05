# Validation Profiles

`validationProfile` identifies the hosted runtime and toolchain used to validate
an answer. It is required for every task and is separate from `targetProfile`,
which records architecture, ABI, and hardware assumptions. Neither field adds
hidden text to a model prompt.

Profile IDs are defined in `src/validation-profiles.mjs`. Tasks, fixture
manifests, raw results, public exports, and validation reports preserve the ID.
Validation reports also record the resolved tool versions and resource limits.

## Profile Registry

| Profile | Intended validation environment |
| --- | --- |
| `c11-host` | C11 compiler, warnings enabled, isolated host execution |
| `go-std` | Go toolchain using only the standard library |
| `node-typescript` | Node.js and TypeScript without external runtime services |
| `node-typescript-postgresql` | Node.js, TypeScript, and isolated PostgreSQL |
| `postgresql` | Isolated PostgreSQL server and client tools |
| `python3-pytest-hypothesis` | Python 3 with isolated pytest and Hypothesis dependencies |
| `python3-stdlib` | Python 3 standard library only |
| `react18-typescript` | React 18, TypeScript, and a deterministic browser-like test runtime |
| `stable-rust` | Stable Rust toolchain using the standard library |

`c11-host` is active for the four executable firmware fixtures. The hosted
profiles are provisional until their fixtures pin exact tool versions,
dependencies, sandbox commands, and resource limits.

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
cannot be represented by an existing profile.

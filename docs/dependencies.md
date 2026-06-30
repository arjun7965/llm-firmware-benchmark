# Dependencies and Validation Toolchains

## Required to Run the Harness

The repository itself has no npm package dependencies. Running benchmarks,
tests, exports, and summaries requires:

- Node.js 22 or newer;
- either the NCode CLI or an OpenAI-compatible HTTP endpoint; and
- model access or credentials required by the selected provider.

Task-language toolchains are not required when the harness only collects model
responses. They are required when an evaluator compiles or executes generated
code.

## Optional Task Validation

| Task ID | Validation dependencies |
| --- | --- |
| `frontend-autocomplete` | Node.js, TypeScript, React 18 and its type declarations; a browser-like test environment for interaction tests |
| `backend-idempotency` | Node.js, TypeScript, Express, `pg`, and PostgreSQL |
| `embedded-ring-buffer` | A C11 compiler with `<stdatomic.h>` support, such as GCC or Clang |
| `firmware-state-machine` | A C11 compiler plus a deterministic mock implementation of the supplied HAL |
| `binary-parser` | A C11 compiler; sanitizers are recommended for executable tests |
| `concurrency-debug` | Python 3 using only its standard library |
| `postgres-pagination` | PostgreSQL server and client tools for schema, query, and `EXPLAIN` validation |
| `testing-property-based` | Python 3, pytest, and Hypothesis |
| `go-graceful-shutdown` | The Go toolchain; only the standard library is used |
| `rust-stream-decoder` | Stable `rustc` to compile; Cargo is recommended for running the unit tests |
| `typescript-singleflight-cache` | Node.js and the TypeScript compiler |
| `webhook-replay-security` | Node.js, TypeScript, Express, `pg`, and PostgreSQL |

These tools are optional because automated answer extraction and compilation are
not yet part of the harness. When validation is performed, record tool versions,
compile flags, package versions, and commands. Use the same environment for all
models in a comparison.

Keep validator-only packages outside the root project or in a future isolated
fixture directory. Do not add runtime dependencies to this dependency-free
harness solely to score one task.

Embedded task toolchains and runtime assumptions are defined separately in
`docs/embedded/target-assumptions.md`.

Fixture manifests declare required tools and commands. `npm run fixtures:check`
validates those declarations but does not install tools or execute generated
code.

Useful version checks include:

```bash
node --version
cc --version
python3 --version
go version
rustc --version
cargo --version
psql --version
```

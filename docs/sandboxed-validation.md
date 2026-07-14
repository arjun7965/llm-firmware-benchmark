# Sandboxed Fixture Validation

Extracted model code is untrusted. Validation is opt-in and Linux-only:

```bash
npm run fixture:extract -- --result results/<task>--<model>.json
npm run fixture:validate -- --task <task>
```

The validator requires Bubblewrap, `prlimit`, and each manifest toolchain as
root-owned, non-writable executables under `/usr`. Compile namespaces also
mount `/etc/alternatives` read-only so attested compiler and linker symlinks
resolve to their root-owned `/usr` targets. Validation fails closed when any
dependency or namespace feature is unavailable; it never falls back to host
execution. It also reads `/etc/os-release` as data and checks the OS ID, release,
and normalized architecture against the concrete environments supported by
the current logical validation-profile revision. Exactly one environment must
match before the validator resolves or executes sandbox tools.

The `node-typescript` and `react18-typescript` profiles pin complete npm
package-locks, root-owned installation paths, and canonical SHA-256 values over
every installed directory and file. Before resolving tools or running
Bubblewrap, the host runner rejects missing, writable, non-root-owned,
symlinked, altered, or oversized package trees. Each verified tree is mounted
read-only at `/workspace/node_modules` in the compile namespace. React
interaction tests also receive their tree in the test namespace because React,
jsdom, and Testing Library are runtime inputs; plain TypeScript cache tests
receive only compiled output and pinned Node.
Other dependency-bearing profiles remain disabled until they define an
equivalent runtime attestation or use a digest-pinned image.

The test namespace executes either a native binary from `build/` or an exact
profile-approved runtime command. `python3-stdlib` mounts its pinned,
root-owned Python 3.12.11 runtime read-only and is enabled for active fixtures.
The `python3-pytest-hypothesis` profile additionally attests and mounts its
hash-pinned package tree for both compilation and test execution, redirects
Hypothesis storage to private `/tmp`, and permits only its fixed pytest command.
`node-typescript` similarly mounts its pinned Node.js runtime for tests.
`react18-typescript` mounts that Node runtime plus the attested React/jsdom
tree and permits only its fixed compiled interaction-test command.
`postgresql` mounts the pinned server/client tree and uses profile-owned
initialize, start, readiness/bootstrap, and stop contracts. Bootstrap creates
a non-superuser database-owner login for candidate SQL. Each phase receives a
new data directory and Unix socket; the server has no TCP listener or network
namespace access and is forcibly reaped with its namespace after the phase.

Ubuntu 24.04 restricts unprivileged user namespaces through AppArmor. The CI
workflow installs `apparmor-profiles` and explicitly loads the distribution's
`bwrap-userns-restrict` policy while leaving the Node validator unprivileged.
Do not disable AppArmor globally to make validation pass.

## Isolation Boundary

Compilation and testing run in separate Bubblewrap namespaces with:

- no network namespace access, capabilities, inherited environment, or home;
- no access to the repository, results, local models, credentials, or `/etc`;
- read-only starter files, mocks, public tests, and extracted answer;
- a temporary writable build directory during compilation, remounted read-only
  for testing;
- profile-specific root and temporary tmpfs sizes, with the root remounted
  read-only after sandbox setup;
- read-only compiler and system libraries during compilation;
- only approved runtime files and either the test binary or interpreter during
  execution; and
- PID namespace teardown and `--die-with-parent` cleanup.

For PostgreSQL, trusted Node orchestration starts the server in its own
Bubblewrap process and runs `psql` in a separate candidate namespace. They
share only the temporary service directory containing the data directory and
private Unix socket. Neither namespace receives the repository, credentials,
`/etc`, a network interface, or a shell-capable runtime. Compile and test use
different service directories, and cleanup removes the complete directory.
Client namespaces mount the service directory read-only; only initialization
and the server namespace can write cluster files.
The separate trusted one-shot `initdb` namespace mounts `/bin` and the
read-only host `/etc/passwd` because `initdb` internally invokes a shell while
verifying its sibling server binary and resolves its effective UID.
The profile's explicit stop command is used by direct trusted calibration;
sandboxed validation instead reaps the server's separate PID namespace.

`prlimit` applies address-space, CPU, output-file, open-file, and core-dump
limits. Node applies a wall timeout and a 1 MiB stdout/stderr cap. A portable
hard process-count limit is intentionally not claimed: applying
`RLIMIT_NPROC` before Bubblewrap can prevent user-namespace creation because
Linux accounts the limit outside the new namespace. The isolated PID namespace,
restricted runtime filesystem, and forced teardown reduce this risk, but
validation of deliberately hostile code should still run on a disposable CI
runner or virtual machine.

## Reports

Each run writes ignored machine-readable output to:

```text
fixtures/<task-id>/build/validation-report.json
```

The report follows `schemas/fixture-validation-report.schema.json` version 1.6.
It records every extracted file's path, byte length, and SHA-256 plus the direct
single-file or canonical bundle SHA-256. It also records the logical
validation-profile revision and contract SHA-256, concrete environment revision
and contract SHA-256, detected host, execution mode, target profile, language,
resolved toolchain and sandbox versions, and any produced native artifact size.
Each phase preserves the
exact compiler or test argv—including compile flags—and records a normalized `passed`, `failed`,
`timed-out`, or `error` outcome alongside timing, limits, diagnostics, exit
status, and signals.

Toolchain version probes execute only root-owned, non-writable programs already
approved by the manifest. Each manifest provides the version argv pinned by its
profile, so tools can use their native interface—for example, `cc --version` or
`go version`—without fallback guessing. Validation fails if a resolved version
does not match the profile. Artifacts remain temporary; reports retain their
fixture-relative path and byte size only. Reports can contain model-controlled
diagnostics and must be reviewed before publication.

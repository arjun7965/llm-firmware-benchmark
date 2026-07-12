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

The `node-typescript` profile pins a complete npm package-lock, a root-owned
installation path, and a canonical SHA-256 over every installed directory and
file. Before resolving tools or running Bubblewrap, the host runner rejects
missing, writable, non-root-owned, symlinked, altered, or oversized package
trees. The verified tree is mounted read-only at `/workspace/node_modules` in
the compile namespace only; tests receive the compiled output and pinned Node
runtime without access to the package tree.
Other npm and PyPI profiles remain disabled until they define an equivalent
runtime attestation or use a digest-pinned image.

The test namespace executes either a native binary from `build/` or an exact
profile-approved runtime command. `python3-stdlib` mounts its pinned,
root-owned Python 3.12.11 runtime read-only and is enabled for active fixtures.
`node-typescript` similarly mounts its pinned Node.js runtime for tests.
`postgresql` declares runtime mounts and command prefixes but remains disabled
until the runner implements its isolated service boundary.

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

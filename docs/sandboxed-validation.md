# Sandboxed Fixture Validation

Extracted model code is untrusted. Validation is opt-in and Linux-only:

```bash
npm run fixture:extract -- --result results/<task>--<model>.json
npm run fixture:validate -- --task <task>
```

The validator requires Bubblewrap, `prlimit`, and each manifest toolchain as
root-owned, non-writable executables under `/usr`. It fails closed when any
dependency or namespace feature is unavailable; it never falls back to host
execution. It also reads `/etc/os-release` as data and checks the OS ID, release,
and normalized architecture against the current validation-profile revision
before resolving or executing sandbox tools.

Profiles with npm or PyPI dependencies are rejected by the current runner
because tool version probes cannot attest installed package contents. They
require a verified lockfile installation or digest-pinned image before
executable fixture validation is enabled.

The test namespace currently executes only a native binary from `build/`.
Consequently, dependency-free interpreter and service profiles are also
rejected. Their fixtures must remain scaffolds until the runner explicitly
mounts the pinned runtime and supports a profile-appropriate test command.

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
- only runtime libraries and the test binary during execution; and
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

The report follows `schemas/fixture-validation-report.schema.json` version 1.4.
It records the extracted-code SHA-256, validation profile revision and contract
SHA-256, target profile, language, resolved toolchain and sandbox versions, and
produced binary size. Each phase preserves the exact compiler or test
argv—including compile flags—and records a normalized `passed`, `failed`,
`timed-out`, or `error` outcome alongside timing, limits, diagnostics, exit
status, and signals.

Toolchain version probes execute only root-owned, non-writable programs already
approved by the manifest. Each manifest provides the version argv pinned by its
profile, so tools can use their native interface—for example, `cc --version` or
`go version`—without fallback guessing. Validation fails if a resolved version
does not match the profile. Artifacts remain temporary; reports retain their
fixture-relative path and byte size only. Reports can contain model-controlled
diagnostics and must be reviewed before publication.

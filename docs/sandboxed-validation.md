# Sandboxed Fixture Validation

Extracted model code is untrusted. Validation is opt-in and Linux-only:

```bash
npm run fixture:extract -- --result results/<task>--<model>.json
npm run fixture:validate -- --task <task>
```

The validator requires Bubblewrap, `prlimit`, and each manifest toolchain as
root-owned, non-writable executables under `/usr`. It fails closed when any
dependency or namespace feature is unavailable; it never falls back to host
execution.

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
- a 32 MiB root tmpfs remounted read-only after sandbox setup;
- a 16 MiB private `/tmp`;
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

The report follows `schemas/fixture-validation-report.schema.json` and records
the extracted-code SHA-256, fixed argv, timing, limits, diagnostics, exit
status, signals, and timeouts. Reports can contain model-controlled diagnostics
and must be reviewed before publication.

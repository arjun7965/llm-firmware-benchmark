# Firmware Host Validation Coverage

Required benchmark scoring for firmware tasks runs against deterministic,
fixture-owned boundaries. It does not require a physical board, debug probe,
vendor simulator, or proprietary SDK. The optional STM32, NXP, and TI
hardware-in-the-loop protocol may supplement this path, but it cannot replace
it; see `hardware-in-the-loop.md`.

## Enforced Invariant

`npm run fixtures:check` fails when a deterministic firmware fixture:

- is not active;
- does not resolve to a hosted validation environment (`host` or a reviewed,
  digest-pinned `oci` environment);
- lacks a nonempty `mocks/README.md` describing its deterministic boundary; or
- supplies mock or simulator assets without using the `mocks/` directory in a
  manifest command.

The existing fixture contract additionally limits commands to profile-approved
toolchains and safe fixture-relative paths. The sandbox runner supplies the
declared inputs with no network access. Together, these rules keep a board or
undeclared SDK out of the required scoring path while allowing optional
cross-compilation and hardware-in-the-loop paths to remain separate.

## Completed Audit

The 2026-08-15 audit covers all 42 deterministic firmware fixtures:

- 33 fixtures execute committed mock or simulator assets;
- 9 fixtures use deterministic caller-owned inputs or immutable evidence and
  document why no runtime mock is required; and
- 42 fixtures resolve to active hosted validation profiles.

Fixtures with executable mock or simulator assets are:

`adc-threshold-watchdog`, `bare-metal-timer`, `brownout-safe-mode`,
`can-controller-recovery`, `dma-cache-coherency`,
`dual-slot-update-recovery`, `fault-crash-record`,
`firmware-state-machine`, `fixed-point-filter-optimization`,
`gpio-edge-debounce`, `i2c-controller-recovery`,
`idempotent-system-init`, `interrupt-deferred-work`,
`interrupt-vector-configuration`, `linker-memory-map`,
`low-power-wake-clock`, `mixed-c-cpp-mmio-safety-review`,
`mpu-fault-containment`, `pwm-synchronized-update`,
`real-time-deadline-budget`, `resilient-serial-service`,
`rtos-event-flags-deadlock`, `rtos-periodic-scheduler`,
`rtos-priority-inversion`, `rtos-queue-semaphore`,
`secure-boot-image-validation`, `secure-maintenance-command`,
`spi-dma-transfer`, `supervised-process-service`,
`timer-capture-overflow`, `timer-dma-handoff`,
`uart-interrupt-driver`, and `watchdog-window-recovery`.

The fixtures that need no runtime mock are:

- `embedded-ring-buffer`, `static-memory-pool`, and
  `fixed-point-stack-budget`, whose state and resource models are caller-owned;
- `cortex-m-hard-fault-debug` and `compiler-trace-regression-debug`, whose
  immutable diagnostic evidence is supplied by the fixture; and
- `binary-parser`, `modbus-rtu-receiver`, `can-transport-reassembly`, and
  `ble-advertising-reassembly`, whose protocol inputs and timestamps are
  deterministic caller-owned values above any physical transport.

The checked counts are emitted as `firmwareHostCoverageCount`,
`firmwareMockAssetCount`, and `firmwareNoRuntimeMockCount`, so adding or
weakening a firmware fixture changes a reviewed, regression-tested result.

# Embedded and Firmware Capability Matrix

## Coverage Rules

- **Gap**: no scored task directly exercises the capability.
- **Partial**: a current task exercises part of the capability.
- **Covered**: calibrated tasks exercise implementation, edge cases, and
  validation across the important variants.

The initial matrix intentionally uses a high bar. Existing tasks are marked
partial until deterministic fixtures and calibration are available.

## Matrix

| Capability ID | Expected evidence | Current coverage | Next representative task |
| --- | --- | --- | --- |
| `bare-metal` | Register access, startup, vector tables, linker and memory-map reasoning | Partial: `bare-metal-timer`, `interrupt-vector-configuration`, `linker-memory-map` | Add linker-script build validation for section placement and image-size budgets |
| `peripheral-drivers` | GPIO, UART, SPI, I2C, ADC, PWM, timers, DMA, watchdogs | Partial: `adc-threshold-watchdog`, `bare-metal-timer`, `can-controller-recovery`, `firmware-state-machine`, `gpio-edge-debounce`, `i2c-controller-recovery`, `pwm-synchronized-update`, `spi-dma-transfer`, `timer-capture-overflow`, `timer-dma-handoff`, `uart-interrupt-driver`, `watchdog-window-recovery` | Add a deterministic CAN-FD controller with error-passive transitions |
| `interrupt-concurrency` | ISR ownership, atomics, critical sections, deferred work | Covered: `embedded-ring-buffer`, `interrupt-deferred-work` | Extend to priority-aware payload ownership with bounded ISR work |
| `rtos` | Tasks, queues, mutexes, events, priority inversion, bounded latency | Covered: `rtos-periodic-scheduler`, `rtos-queue-semaphore`, `rtos-priority-inversion`, `rtos-event-flags-deadlock` | Add a multi-core affinity and interrupt-to-task handoff task after the single-core contracts are calibrated |
| `embedded-linux` | POSIX devices, threads, processes, signals, and constrained services | Gap | Implement a resilient serial service with shutdown and reconnect behavior |
| `constrained-memory` | Static allocation, pools, stack bounds, alignment, cache and DMA rules | Covered: `embedded-ring-buffer`, `static-memory-pool`, `fixed-point-stack-budget`, `dma-cache-coherency` | Add a memory-region partitioning task only after calibrating allocator fragmentation variants |
| `protocols` | Framing, parsers, CRCs, timeouts, CAN, Modbus, and malformed input | Covered: `binary-parser`, `modbus-rtu-receiver`, `can-transport-reassembly`, `ble-advertising-reassembly`, `can-controller-recovery` | Add encrypted transport/session negotiation only after protocol variants are calibrated |
| `reliability` | Watchdogs, brownouts, fault recovery, persistent state, safe mode | Covered: `watchdog-window-recovery`, `brownout-safe-mode`, `fault-crash-record`, `idempotent-system-init` | Calibrate retained-state and multi-reset variants across model families |
| `boot-update` | Image validation, rollback, version policy, interrupted updates | Covered: `secure-boot-image-validation`, `dual-slot-update-recovery` | Calibrate anti-rollback storage and multi-image dependency variants across model families |
| `power-real-time` | Sleep, wake sources, clocks, deadlines, jitter, execution budgets | Covered: `low-power-wake-clock`, `real-time-deadline-budget` | Calibrate combined clock-transition and multi-rate workload variants across model families |
| `debugging` | Diagnostics, traces, register dumps, map files, and disassembly | Gap | Diagnose a hard fault from a supplied exception frame and map excerpt |
| `language-safety` | Undefined behavior, integer conversion, ownership, RAII, portability | Partial: `embedded-ring-buffer`, `binary-parser` | Review mixed C/C++ MMIO code for lifetime and aliasing defects |
| `firmware-security` | Untrusted input, debug access, updates, secrets, MPU, fault injection | Partial: `binary-parser` | Harden a boot command parser and debug-unlock policy |
| `resource-optimization` | Code size, RAM, stack, energy, and bounded execution tradeoffs | Partial: `embedded-ring-buffer`, `fixed-point-stack-budget` | Optimize a fixed-point filter under explicit error and cycle budgets |

## Selection Policy

Prioritize gaps before adding variants of partially covered capabilities. New
tasks should cover one primary capability and at most two secondary
capabilities, use a profile from `target-assumptions.md`, and define observable
success criteria. Avoid tasks whose correctness depends on undocumented vendor
behavior or physical hardware unavailable to evaluators.

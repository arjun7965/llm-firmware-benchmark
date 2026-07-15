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
| `bare-metal` | Register access, startup, vector tables, linker and memory-map reasoning | Partial: `bare-metal-timer` | Add startup, vector-table, and linker-symbol behavior |
| `peripheral-drivers` | GPIO, UART, SPI, I2C, ADC, PWM, timers, DMA, watchdogs | Partial: `bare-metal-timer`, `firmware-state-machine`, `uart-interrupt-driver`, `spi-dma-transfer` | Implement an I2C controller with arbitration-loss and timeout recovery |
| `interrupt-concurrency` | ISR ownership, atomics, critical sections, deferred work | Partial: `embedded-ring-buffer` | Repair an ISR/main race with nested-interrupt assumptions |
| `rtos` | Tasks, queues, mutexes, events, priority inversion, bounded latency | Partial: `rtos-priority-inversion` | Diagnose and repair priority inversion across a supplied RTOS queue and mutex API |
| `embedded-linux` | POSIX devices, threads, processes, signals, and constrained services | Gap | Implement a resilient serial service with shutdown and reconnect behavior |
| `constrained-memory` | Static allocation, pools, stack bounds, alignment, cache and DMA rules | Partial: `embedded-ring-buffer` | Build a fixed-block allocator with deterministic exhaustion behavior |
| `protocols` | Framing, parsers, CRCs, timeouts, CAN, Modbus, and malformed input | Partial: `binary-parser` | Implement a streaming CAN transport reassembler with timeout recovery |
| `reliability` | Watchdogs, brownouts, fault recovery, persistent state, safe mode | Partial: `firmware-state-machine` | Design idempotent boot recovery around interrupted persistent writes |
| `boot-update` | Image validation, rollback, version policy, interrupted updates | Gap | Implement dual-slot update selection using a supplied flash model |
| `power-real-time` | Sleep, wake sources, clocks, deadlines, jitter, execution budgets | Gap | Schedule sampling across sleep states with wrap-safe deadlines |
| `debugging` | Diagnostics, traces, register dumps, map files, and disassembly | Gap | Diagnose a hard fault from a supplied exception frame and map excerpt |
| `language-safety` | Undefined behavior, integer conversion, ownership, RAII, portability | Partial: `embedded-ring-buffer`, `binary-parser` | Review mixed C/C++ MMIO code for lifetime and aliasing defects |
| `firmware-security` | Untrusted input, debug access, updates, secrets, MPU, fault injection | Partial: `binary-parser` | Harden a boot command parser and debug-unlock policy |
| `resource-optimization` | Code size, RAM, stack, energy, and bounded execution tradeoffs | Partial: `embedded-ring-buffer` | Optimize a fixed-point filter under explicit error and cycle budgets |

## Selection Policy

Prioritize gaps before adding variants of partially covered capabilities. New
tasks should cover one primary capability and at most two secondary
capabilities, use a profile from `target-assumptions.md`, and define observable
success criteria. Avoid tasks whose correctness depends on undocumented vendor
behavior or physical hardware unavailable to evaluators.

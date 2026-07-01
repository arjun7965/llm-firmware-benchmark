# Embedded Target Assumptions

## Required Assumption Fields

Every embedded or firmware task must state or reference:

1. execution profile: hosted simulation, bare-metal, RTOS, or embedded Linux;
2. architecture and relevant ISA features;
3. ABI and data model, including important type widths and alignment;
4. byte order and peripheral register byte order;
5. language version, compiler mode, extensions, and required warnings;
6. concurrency model: cores, ISR nesting, priorities, and atomic guarantees;
7. memory model: map, MMIO semantics, cache, DMA, and allocation policy;
8. time model: tick source, units, wraparound, deadlines, and clock changes;
9. available HAL, RTOS, or operating-system APIs and their completion semantics;
10. resource budgets for RAM, stack, code size, execution time, and power;
11. validation environment, fixtures, toolchain, and permitted dependencies; and
12. safety and security boundaries, including which inputs are untrusted.

Unspecified behavior must not be required to score a response. Task-specific
text overrides a profile, but every override must be explicit.

`targetProfile` is metadata consumed by validation and result reporting.
Provider adapters send only the task's `prompt`, so any assumption the model
must know also belongs in the prompt; profiles do not create hidden context.

## Current Profiles

### `portable-c11`

Hosted C11 validation with 8-bit bytes and standard fixed-width integer types.
No packed-struct, unaligned-access, or compiler-extension assumptions are
allowed. Endianness-sensitive data must be decoded explicitly. Host GCC or Clang
may be used with warnings enabled; sanitizers are optional validation aids.

### `c11-lock-free-spsc`

Extends `portable-c11` with one single-core interrupt producer and one main-loop
consumer. Producer and consumer calls are not nested within their own context.
The selected atomic index type must be lock-free on the intended target; cache
maintenance and DMA are outside this profile. Tests may use host threads only as
a behavioral approximation of interleaving.

### `c11-mocked-hal`

Extends `portable-c11` with deterministic host-side HAL fakes and a modulo
`uint32_t` millisecond clock. Tests control completion, failure, and time
advancement. A task must define whether completion remains observable until
acknowledged and whether failed operations require explicit reset.

## Planned Profiles

### `armv7m-bare-metal`

Little-endian ARMv7-M, AAPCS/EABI, single core, privileged bare-metal execution,
no heap by default, and documented mock MMIO. Tasks must state interrupt
priorities, FPU availability, memory map, and whether exclusive accesses are
permitted. Vendor SDKs are unavailable unless a task supplies the required API.

### `rv32-bare-metal`

Little-endian RV32IMAC with ILP32 ABI, single-hart bare-metal execution, no heap
by default, and documented mock MMIO. Tasks must state trap behavior, atomic
extension use, alignment behavior, memory map, and timer source.

### `generic-rtos`

Single-core preemptive RTOS simulation with a fully supplied task, queue,
synchronization, ISR, and time API. Tasks must define priority direction,
scheduling policy, ISR-callable operations, timeout units, and shutdown model;
no proprietary RTOS knowledge may be assumed.

### `embedded-linux-posix`

Little-endian Linux user space using a stated POSIX and language version.
Tasks must define device interfaces, permissions, thread/process model, signal
behavior, filesystem durability assumptions, and service resource limits.

## Current Task Mapping

| Task ID | Target profile | Task-specific assumptions |
| --- | --- | --- |
| `embedded-ring-buffer` | `c11-lock-free-spsc` | Caller-owned power-of-two storage; drop-new overflow; ISR producer; main-loop consumer |
| `firmware-state-machine` | `c11-mocked-hal` | Supplied asynchronous I2C API; 32-bit millisecond clock |
| `binary-parser` | `portable-c11` | Untrusted unaligned bytes; explicit little-endian fields |

Planned profiles become active only when a committed task supplies its fixtures,
rubric, dependency entry, and validation commands.

# Interrupt-Vector Configuration

## Objective

Assess startup-time construction and publication of a RAM exception-vector
table, plus safe live updates to external IRQ handlers on a fictional
Cortex-M3. Implement the API declared by
`fixtures/interrupt-vector-configuration/starter/interrupt_vector.h` using
only the supplied table, SCB, NVIC, barrier, and interrupt-mask accessors.

## Target Assumptions

Target profile: `armv7m-bare-metal`. The fixture models a single-core
little-endian Cortex-M3 with AAPCS/EABI, a linker-reserved 128-byte-aligned RAM
vector table, eight external NVIC IRQs, and no heap, FPU, cache, DMA, RTOS, or
vendor SDK. Reset-time initialization begins with interrupts masked. Live table
and NVIC changes run in privileged thread mode and must preserve the exact
global interrupt state. The supplied linker-address accessor abstracts the
target memory map for deterministic host validation.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** The implementation validates stack and Thumb handler values, creates the exact 24-entry table layout, publishes the linker address to VTOR, and installs/enables the requested external IRQ.
- 1 point — **Bounded resource use:** The answer uses caller-owned fixed storage, has only a fixed-size table-initialization loop, and adds no allocation, polling, retry, or mutable global state.
- 2 points — **Timing behavior:** Startup disables and clears NVIC state before table publication, fills entries in order, synchronizes before and after VTOR, and clears an IRQ pending bit before enabling it.
- 2 points — **Concurrency safety:** Reset initialization does not alter the already-masked global state; live install and enable operations save-disable exactly once and restore the exact caller state.
- 1 point — **Fault recovery:** Invalid inputs and unaligned/zero linker addresses cause no table, SCB, NVIC, barrier, or interrupt-mask side effects; reinitialization replaces stale vectors and IRQ state.
- 1 point — **Portability:** The code uses freestanding portable C11 and supplied accessors rather than direct MMIO/table fields, pointer-to-address casts, inline assembly, or a vendor SDK.
- 1 point — **Clarity and validation:** The explanation covers startup/vector publication and live-update ordering; tests cover layout, invalid calls, reinitialization, IRQ-slot bounds, stale-pending order, and interrupt restoration.

Publishing VTOR before synchronizing the completed table, accepting an even
handler address, updating a live vector without a critical section, or enabling
an IRQ before clearing stale pending state does not receive the associated
timing or concurrency credit.

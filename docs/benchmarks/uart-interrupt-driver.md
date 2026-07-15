# UART Interrupt Driver

## Objective

Assess an interrupt-driven UART driver that bounds ISR work while safely sharing
fixed-capacity receive and transmit queues with foreground code.

Implement the API declared by
`fixtures/uart-interrupt-driver/starter/uart_driver.h` against the supplied
fictional UART0 MMIO and interrupt-mask boundary.

## Target Assumptions

Target profile: `armv7m-bare-metal`. The task uses a single-core little-endian
Cortex-M3 with AAPCS/EABI, no heap, DMA, cache, FPU, RTOS, vendor SDK, or host
threads. UART0 has a non-nested IRQ. Foreground functions may be interrupted,
so they preserve the exact state returned by the supplied save-disable accessor.
Each IRQ invocation snapshots status once and services at most one received byte
and one transmitted byte.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Initialization programs UART0 in the declared order, TX writes preserve FIFO order, and RX reads preserve FIFO order through the full eight-byte capacity.
- 1 point — **Bounded resource use:** The answer uses only caller-owned fixed storage; no allocation, unbounded loop, global state, or busy wait is introduced.
- 2 points — **Timing behavior:** Foreground writes defer transmission to TX-empty interrupts, and each ISR invocation performs no more than one RX and one TX data operation.
- 2 points — **Concurrency safety:** Foreground queue/statistics operations mask and restore the exact prior interrupt state, while the non-nested ISR never attempts to manage that state itself.
- 1 point — **Fault recovery:** RX overflow is counted while hardware data is drained, framing/overrun bits are acknowledged and exposed once, and invalid calls have no MMIO or guard side effects.
- 1 point — **Portability:** The implementation uses only portable freestanding C11, fixed-width types, and the supplied MMIO/interrupt accessors.
- 1 point — **Clarity and validation:** The explanation covers queue ownership, bounded ISR work, error handling, and tests cover initialization, capacity, ordering, overflow, errors, and interrupt-mask restoration.

Direct UART data writes from foreground code, polling loops, enabling TX IRQ at
initialization, or unconditionally enabling interrupts after a foreground call
do not receive the corresponding timing or concurrency-safety credit.

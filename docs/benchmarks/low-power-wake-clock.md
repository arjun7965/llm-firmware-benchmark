# Low-Power Wake and Clock Transition

## Objective

Assess a bounded Cortex-M power manager that arms explicit wake sources, enters
idle or deep sleep in a safe order, and restores the run clock after a real
wake without mistaking a stale or unconfigured latch for a wake event.

Implement the API declared by
`fixtures/low-power-wake-clock/starter/low_power_wake_clock.h` using only the
supplied opaque PWRCLK0 and interrupt-mask accessors.

## Target Assumptions

Target profile: `armv7m-bare-metal`. The fixture models a single-core
little-endian Cortex-M3 with AAPCS/EABI, no heap, cache, FPU, DMA, RTOS,
vendor SDK, or host threads. PWRCLK0 provides a 48 MHz run clock and a 4 MHz
deep-sleep clock. RTC, GPIO, and UART are independently latched wake sources;
UART is deliberately unavailable in deep sleep. Foreground power transitions
must preserve the caller's exact global interrupt state.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Initialization establishes a known awake PWRCLK0 state; valid idle/deep requests retain their exact mask and mode; and resume returns GPIO, RTC, or UART only for an enabled source with the stated priority.
- 1 point — **Bounded resource use:** The implementation uses caller-owned state, one status snapshot per resume, and no allocation, polling loop, retry loop, or unbounded work.
- 2 points — **Timing behavior:** Deep sleep selects the 4 MHz clock only after stale latches are cleared and before entry; every successful wake returns the 48 MHz run clock before normal operation; and the clock-rate query reflects the active sleep state.
- 2 points — **Concurrency safety:** Valid prepare/resume calls save-disable and restore the exact interrupt state once, with wake sources disabled during transition and armed before the sleep mode is selected.
- 1 point — **Fault recovery:** Invalid masks, deep UART requests, uninitialized calls, and duplicate sleep arming have no effects; a raw latch outside the configured mask keeps the system asleep; successful recovery disables configured wakes and clears only observed configured latches.
- 1 point — **Portability:** The answer uses freestanding C11 and only fixture-owned PWRCLK0/interrupt accessors, without direct MMIO fields, pointer casts, inline assembly, or vendor APIs.
- 1 point — **Clarity and validation:** The explanation covers source filtering, GPIO-over-RTC-over-UART priority, stale-latch handling, clock transitions, and deterministic tests for ordering and interrupt restoration.

Do not award clock-transition or recovery credit for retaining stale latches,
allowing UART in deep sleep, entering a sleep mode before its wake mask is
armed, treating an unconfigured raw latch as a wake event, or leaving the
sleep clock selected after resume.

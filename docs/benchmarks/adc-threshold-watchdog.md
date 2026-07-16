# ADC Threshold/Watchdog Recovery

## Objective

Assess a bounded ADC threshold monitor that starts one asynchronous 12-bit
conversion at a time, handles its terminal hardware status in a non-nested ISR,
and deterministically resets the peripheral after an overrun or foreground
timeout.

Implement the API declared by
`fixtures/adc-threshold-watchdog/starter/adc_watchdog.h` using only the
fixture-owned opaque ADC0 and interrupt-mask accessors.

## Target Assumptions

Target profile: `armv7m-bare-metal`. The fixture models a single-core
little-endian Cortex-M3 with AAPCS/EABI, no heap, cache, FPU, RTOS, vendor SDK,
DMA, or host threads. ADC0 provides a 12-bit sample plus independent
end-of-conversion, analog-window-watchdog, and overrun latches. Its ISR is
non-nested; foreground calls use the supplied interrupt save/restore boundary
to coordinate with it. The foreground timeout intentionally owns a conversion
whose status latched but whose ISR did not start.

## Scoring

Scoring profile: `firmware-v1`.

- 3 points — **Functional correctness:** Valid initialization programs the exact
  threshold window and ready control; start, event consumption, and
  reinitialization state are correct; the ISR takes one status snapshot,
  samples data once for EOC/AWD, gives AWD precedence over EOC, ignores
  unrelated bits, and gives overrun precedence without reading data.
- 1 point — **Bounded resource use:** The implementation uses only
  caller-owned state, performs bounded accessor calls, and has no allocation,
  busy wait, retry loop, or unbounded work.
- 1 point — **Timing behavior:** Timeout uses wrap-safe elapsed arithmetic at
  the exact 25 ms boundary and deliberately owns an unserved latched status.
- 1 point — **Concurrency safety:** ISR and foreground responsibilities are
  distinct, and every initialized foreground call preserves the exact caller
  interrupt state while start clears stale flags under that boundary.
- 2 points — **Fault recovery:** Overrun and timeout stop ADC delivery, clear
  stale flags, latch their fault event, and require an explicit deterministic
  reconfiguration before another start.
- 1 point — **Portability:** The answer uses freestanding C11 and only the
  fixture-owned ADC0 and interrupt accessors, without direct registers,
  pointer casts, inline assembly, or vendor APIs.
- 1 point — **Clarity and validation:** The explanation covers threshold
  classification, terminal-status priority, timeout ownership, fault recovery,
  interrupt restoration, and deterministic tests.

Treating an out-of-window sample as in-window, reading data on overrun,
starting before a result is consumed, using a non-wrapping deadline, or
recovering a healthy instance are substantial correctness defects.

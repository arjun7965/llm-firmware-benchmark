# MPU Fault Containment

## Objective

Assess least-privilege Cortex-M3 MPU setup and fail-closed fault handling
through opaque MPU0/security-controller boundaries.

Implement `fixtures/mpu-fault-containment/starter/mpu_fault_containment.h`.
The implementation includes `<stdbool.h>`, `<stddef.h>`, and `<stdint.h>` so
the complete API and its required null-pointer checks compile freestanding.
The four-region policy is exact: executable read-only flash; read/write,
execute-never SRAM; privileged-only, execute-never key vault overriding SRAM;
and a highest-priority no-access, execute-never stack guard.

## Target assumptions

Target profile: `armv7m-bare-metal`. The supplied bases and sizes satisfy
Cortex-M3 MPU alignment/size rules. Initialization is safe-first and models
deterministic clock, voltage, glitch, and MPU fault latches. Fault injection is
a latch/containment test model, not proof of resistance to DMA or laboratory
attacks. Runtime recovery requires reboot or reinitialization.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Exact region permissions, priorities,
  barriers, enable/readback, initialization, IRQ, and event behavior.
- 1 point — **Bounded resource use:** Four bounded regions, no heap, polling,
  retries, mutable globals, or direct register access.
- 1 point — **Timing behavior:** Safe-first order, fixed programming/readback count,
  and one runtime status snapshot are respected.
- 1 point — **Concurrency safety:** Foreground event consumption preserves the exact
  interrupt state and the ISR does not use foreground synchronization.
- 2 points — **Fault recovery:** All pre-, during-, and post-configuration faults and
  readback mismatches remain contained; runtime faults contain before clear and
  cannot recover in place.
- 2 points — **Portability:** Freestanding C11 uses opaque accessors and supplied
  fixed-width constants without host pointer layout, heap, or UB assumptions.
- 1 point — **Clarity and validation:** Explanation distinguishes MPU policy from
  physical fault resistance and tests prove event ordering and readback.

The prompt is self-contained: its API, opaque accessor signatures, exact
addresses, sizes, priorities, permissions, barriers, safe-first order,
fault-injection points, configuration-containment action, and exact interrupt
save/restore behavior are normative. The public tests and mutations are
authoritative for deterministic rejection.

## Calibration

Run `npm run fixture:mpu-fault:self-test` to exercise the trusted reference
and its seven public test groups. The C mutation suite rejects all 34
compile-valid controlled defects for region policy, readback, injected faults,
containment ordering, event handling, and initialization state publication.

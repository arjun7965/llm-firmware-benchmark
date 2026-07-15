# I2C Controller Recovery

## Objective

Assess a bounded, asynchronous I2C controller that drives one write
transaction through START, address acknowledgement, and data acknowledgement,
then recovers deterministically from arbitration loss, bus errors, NACK, and a
wrap-safe deadline.

Implement the API declared by
`fixtures/i2c-controller-recovery/starter/i2c_controller.h` using only the
supplied opaque I2C0 accessors.

## Target Assumptions

Target profile: `armv7m-bare-metal`. The fixture models a single-core
little-endian Cortex-M3 with AAPCS/EABI, no heap, cache, FPU, RTOS, vendor SDK,
or host threads. All calls run from privileged foreground code; the caller owns
the controller and payload storage. I2C hardware completion is modeled by one
deterministic status snapshot per poll call. Arbitration loss means this
controller no longer owns the bus, so it must not transmit STOP.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Initialization and bounded writes use the declared 7-bit address range, issue START, transmit the shifted write address, and send every payload byte before STOP.
- 1 point — **Bounded resource use:** The implementation uses caller-owned state and a maximum four-byte payload, with no allocation, busy wait, unbounded loop, or automatic retry.
- 2 points — **Timing behavior:** Every active poll takes one status snapshot; normal events advance in START/address/data order; timeout arithmetic is correct over `uint32_t` wraparound and an on-time completion beats a simultaneous deadline.
- 2 points — **Concurrency safety:** A terminal result blocks reuse until taken, stale status is cleared before each START, and all invalid or inactive calls avoid I2C access and state mutation.
- 1 point — **Fault recovery:** Arbitration loss clears controller state without STOP, bus error and NACK release the bus with STOP, timeout requests STOP before clearing status, and every terminal path permits recovery after the result is taken.
- 1 point — **Portability:** The answer uses freestanding C11 and only fixture-owned accessors for the opaque I2C0 peripheral, with no direct register access, pointer casts, inline assembly, or vendor SDK.
- 1 point — **Clarity and validation:** The explanation covers the protocol state machine, fault priorities, and recovery ordering; tests cover normal write ordering, stale status, result consumption, arbitration loss, NACK/bus error, and wrap-safe timeout.

Sending STOP after arbitration loss, accepting a reserved/invalid address,
transmitting an unshifted write address, reporting completion without STOP, or
timing out one tick late does not receive the associated correctness, timing, or
recovery credit.

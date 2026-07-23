# Modbus RTU Receiver

## Objective

Assess a bounded stateful Modbus RTU request receiver that frames a byte stream
after a silent interval, checks the reflected Modbus CRC, validates a narrow
request PDU set, and recovers after malformed or incomplete traffic.

Implement the API declared by
fixtures/modbus-rtu-receiver/starter/modbus_rtu_receiver.h.

## Target Assumptions

Target profile: `portable-c11`. Byte timestamps are deterministic unsigned
32-bit ticks, so elapsed-time arithmetic must remain correct across wrapping.
The fixture receives complete bytes from a lower layer; it does not model a
UART, heap, RTOS, DMA, cache, or vendor SDK.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Collect exactly one eight-byte request, decode big-endian register fields, accept only documented unit addresses/functions/read counts, and require the Modbus CRC byte order.
- 1 point — **Bounded resource use:** Use only caller-owned fixed frame and request storage with no allocation, unbounded buffering, retry, or mutable global state.
- 2 points — **Timing behavior:** Do not finish before the four-tick silence interval, finish exactly at it, require polling before a stale next byte, and use wrap-safe elapsed arithmetic.
- 0 points — **Concurrency safety:** This foreground-only byte receiver has no ISR, thread, or RTOS interaction to assess.
- 2 points — **Fault recovery:** Publish distinct timeout, bad-CRC, and malformed results; block reuse until result consumption; and recover deterministically afterwards.
- 1 point — **Portability:** Use freestanding C11 fixed-width byte operations without alignment assumptions or struct casts over protocol bytes.
- 2 points — **Clarity and validation:** Explain the framing/result state machine and cover valid read/write requests, wrap, partial traffic, CRC/PDU rejection, overflow, and recovery.

Completing a short request as valid, accepting a zero read count, swapping the
CRC bytes, or accepting new traffic before a terminal result is taken loses
the associated correctness, timing, or recovery credit.

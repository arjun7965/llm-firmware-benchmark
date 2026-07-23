# BLE Advertising Reassembly

## Objective

Assess bounded BLE-style advertising fragment reassembly plus defensive parsing
of nested advertising-data structures. One report must bind all fragments to an
advertiser address, enforce sequence order and expiry, then require one flags
field and one complete local-name field.

Implement the API declared by
fixtures/ble-advertising-reassembly/starter/ble_advertising_reassembly.h.

## Target Assumptions

Target profile: `portable-c11`. Fragments are deterministic lower-layer values
with six-byte advertiser addresses, eight-byte payload limits, and unsigned
32-bit millisecond timestamps. This evaluates packet-layer parsing only, not
radio timing, link-layer CRC, encryption, or a hardware controller.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Reassemble only ordered fragments from one advertiser and parse valid length/type/value advertising data into the specified flags and local name report.
- 1 point — **Bounded resource use:** Use only the caller-owned 24-byte reassembly and eight-byte name buffers with no allocation, unbounded buffering, retry, or mutable global state.
- 2 points — **Timing behavior:** Reject stale continuations until polling publishes timeout exactly at the 50-millisecond wrap-safe deadline, without waiting.
- 0 points — **Concurrency safety:** The deterministic fragment receiver has no ISR, thread, or RTOS interaction to assess.
- 2 points — **Fault recovery:** Detect wrong address/sequence, capacity overflow, malformed nested lengths, missing required fields, and duplicate flags; require result consumption before reuse.
- 1 point — **Portability:** Use freestanding C11 byte arrays and explicit bounds checks without packed structs, unaligned casts, or radio-SDK access.
- 2 points — **Clarity and validation:** Explain fragment state, advertiser binding, AD parsing, terminal results, and focused valid, malformed, overflow, timeout, and recovery tests.

Mixing advertisers, accepting missing/duplicate required AD fields, treating
truncated lengths as valid, or retaining a consumed result loses the
corresponding correctness or recovery credit.

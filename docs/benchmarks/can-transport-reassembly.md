# CAN Transport Reassembly

## Objective

Assess a bounded protocol-layer reassembler above complete classic-CAN frames.
It accepts a fixed source identifier, single frames, first frames, and ordered
consecutive frames; it must detect malformed framing and expire an incomplete
message without polling hardware.

Implement the API declared by
fixtures/can-transport-reassembly/starter/can_transport_reassembly.h.

## Target Assumptions

Target profile: `portable-c11`. The lower CAN controller has already validated
and delivered each classic-CAN frame, including its hardware CRC. Timestamps
are caller-supplied unsigned 32-bit milliseconds and may wrap. The task has no
heap, RTOS, driver register, cache, DMA, or vendor-SDK dependency.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Validate the transport identifier/DLC, decode single and 8-to-24-byte segmented messages, retain bytes in order, and enforce exact consecutive sequence numbers.
- 1 point — **Bounded resource use:** Use the fixed 24-byte caller-owned message and assembly buffers with no allocation, queue, retry, or mutable global state.
- 2 points — **Timing behavior:** Expire an active transfer exactly at the ten-millisecond deadline with wrap-safe arithmetic and never busy-wait.
- 0 points — **Concurrency safety:** Complete CAN frames are supplied to a single foreground receiver; this task has no shared ISR or thread access to assess.
- 2 points — **Fault recovery:** Turn invalid source, DLC, frame type, sequence, length, or overlapping start traffic into one terminal malformed result and permit clean reuse only after consumption.
- 1 point — **Portability:** Use freestanding C11 and explicit byte/length arithmetic without packed structs, direct controller access, or architecture-specific code.
- 2 points — **Clarity and validation:** Explain transport framing, sequence/DLC invariants, timeout ownership, and tests for single frames, segmentation, malformed recovery, and tick wrap.

Accepting padded final frames, a wrong sequence, an eight-byte first frame as
invalid, or a post-deadline continuation loses the corresponding credit.

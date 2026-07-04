# Binary Parser

## Objective

Assess defensive parsing of an untrusted byte frame in portable C.

Implement the API declared by
`fixtures/binary-parser/starter/binary_parser.h`. CRC validation uses
CRC-16/CCITT-FALSE with polynomial `0x1021`, initial value `0xFFFF`, no
reflection, and no final XOR.

## Target Assumptions

Target profile: `portable-c11`. See `docs/embedded/target-assumptions.md`.

## Scoring

Scoring profile: `firmware-v1`.

- 4 points — **Functional correctness:** Valid frames have correctly decoded fields and return the required non-owning payload view.
- 1 point — **Bounded resource use:** Parsing operates directly on the input without allocation or an owned payload copy.
- 0 points — **Timing behavior:** This task specifies no deadline, latency, blocking, or real-time scheduling contract.
- 0 points — **Concurrency safety:** The supplied stateless parser API has no shared ISR, thread, or RTOS interaction to assess.
- 2 points — **Fault recovery:** Validation order, CRC verification, exact-length rejection, payload limits, distinct error results, and output clearing match the malformed-input contract.
- 2 points — **Portability:** Unaligned input, integer arithmetic, and byte order are handled explicitly without struct casts or undefined behavior.
- 1 point — **Clarity and validation:** The implementation is reviewable and focused tests cover valid frames and every rejection path.

Casting the input buffer to a packed or ordinary struct loses the portability
and alignment points.

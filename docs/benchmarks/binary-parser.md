# Binary Parser

## Objective

Assess defensive parsing of an untrusted byte frame in portable C.

## Scoring

- 3 points — Every length calculation and buffer access is bounds-checked safely.
- 2 points — Magic, version, little-endian fields, and CRC coverage are implemented correctly.
- 2 points — Truncation, trailing data, oversize payloads, and corrupt frames get distinct errors.
- 2 points — The parser is alignment-safe, overflow-safe, and returns a valid non-owning view.
- 1 point — Focused tests cover valid frames and each rejection path.

Casting the input buffer to a packed or ordinary struct loses the portability
and alignment points.

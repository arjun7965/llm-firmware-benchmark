# Rust Stream Decoder

## Objective

Assess incremental parsing, resynchronization, and bounded resource use in safe,
stable Rust.

The scaffold contract is under `fixtures/rust-stream-decoder/`. The supplied
API fixes `StreamDecoder::push` as an owned-payload interface and exposes
`buffered_len` so tests can observe retained input. Validator-owned tests are
compiled through a fixed crate root; model output cannot omit them.

## Scoring

- 3 points — Arbitrary chunks and multiple frames are decoded with correct state preservation.
- 2 points — Payload and buffer growth are strictly bounded for hostile lengths and garbage.
- 2 points — Bad magic, checksum failures, and partial magic resynchronize correctly.
- 2 points — Stable Rust code is panic-free, idiomatic, and supported by focused tests.
- 1 point — Time complexity and maximum memory use are stated accurately.

Discarding a possible trailing first magic byte during resynchronization loses
partial-boundary correctness credit.

The trusted reference and eight controlled mutations are calibrated with:

```bash
npm run fixture:rust-decoder:self-test
```

The fixture remains a scaffold until the exact Rust/Cargo 1.87.0 profile can
run inside the sandbox namespace.

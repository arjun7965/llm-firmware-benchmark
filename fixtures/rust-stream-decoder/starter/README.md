# Starter Contract

`stream_decoder_api.rs` defines the exact API implemented by the extracted
answer. `test_harness.rs` is the validator-owned crate root that always imports
the answer and public tests; model output cannot omit the test module.

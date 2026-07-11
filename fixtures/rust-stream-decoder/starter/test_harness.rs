#[path = "stream_decoder_api.rs"]
mod decoder_api;

#[path = "../generated/answer.rs"]
mod answer;

#[cfg(test)]
#[path = "../tests/public/test_stream_decoder.rs"]
mod public_tests;

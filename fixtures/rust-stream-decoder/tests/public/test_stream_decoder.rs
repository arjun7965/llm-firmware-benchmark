use crate::answer::StreamDecoder;
use crate::decoder_api::{Decoder, MAX_BUFFERED, MAX_PAYLOAD};

fn encode(payload: &[u8]) -> Vec<u8> {
    assert!(payload.len() <= MAX_PAYLOAD);
    let mut frame = Vec::with_capacity(payload.len() + 7);
    frame.extend_from_slice(&[0xCA, 0xFE]);
    frame.extend_from_slice(&(payload.len() as u32).to_be_bytes());
    frame.extend_from_slice(payload);
    frame.push(payload.iter().copied().fold(0u8, |sum, byte| sum ^ byte));
    frame
}

#[test]
fn decodes_empty_and_multiple_frames() {
    let mut input = encode(&[]);
    input.extend_from_slice(&encode(&[0x10, 0x20, 0x30]));
    let mut decoder = StreamDecoder::new();
    assert_eq!(decoder.push(&input), vec![vec![], vec![0x10, 0x20, 0x30]]);
    assert_eq!(decoder.buffered_len(), 0);
}

#[test]
fn accepts_every_two_chunk_split() {
    let encoded = encode(&[0x01, 0x02, 0x03, 0x04, 0x05]);
    for split in 0..=encoded.len() {
        let mut decoder = StreamDecoder::new();
        let mut frames = decoder.push(&encoded[..split]);
        frames.extend(decoder.push(&encoded[split..]));
        assert_eq!(frames, vec![vec![0x01, 0x02, 0x03, 0x04, 0x05]]);
        assert_eq!(decoder.buffered_len(), 0);
    }
}

#[test]
fn decodes_bytewise_input() {
    let encoded = encode(&[0xA1, 0xB2, 0xC3]);
    let mut decoder = StreamDecoder::new();
    let mut frames = Vec::new();
    for byte in encoded {
        frames.extend(decoder.push(&[byte]));
    }
    assert_eq!(frames, vec![vec![0xA1, 0xB2, 0xC3]]);
}

#[test]
fn preserves_partial_magic_after_garbage() {
    let encoded = encode(&[0x44]);
    let mut decoder = StreamDecoder::new();
    assert!(decoder.push(&[0x00, 0xCA]).is_empty());
    assert_eq!(decoder.buffered_len(), 1);
    assert_eq!(decoder.push(&encoded[1..]), vec![vec![0x44]]);
}

#[test]
fn preserves_partial_magic_after_rejected_frame() {
    let mut bad = encode(&[0x01]);
    *bad.last_mut().expect("encoded frame has checksum") = 0xCA;
    let valid = encode(&[0x45]);
    let mut decoder = StreamDecoder::new();
    assert!(decoder.push(&bad).is_empty());
    assert_eq!(decoder.buffered_len(), 1);
    assert_eq!(decoder.push(&valid[1..]), vec![vec![0x45]]);
}

#[test]
fn rejects_bad_checksum_and_continues_in_same_push() {
    let mut bad = encode(&[0x11, 0x22]);
    *bad.last_mut().expect("encoded frame has checksum") ^= 0xFF;
    bad.extend_from_slice(&encode(&[0x33, 0x44]));
    let mut decoder = StreamDecoder::new();
    assert_eq!(decoder.push(&bad), vec![vec![0x33, 0x44]]);
    assert_eq!(decoder.buffered_len(), 0);
}

#[test]
fn recovers_an_embedded_frame_after_bad_checksum() {
    let embedded = encode(&[0x66, 0x77]);
    let mut outer = encode(&embedded);
    *outer.last_mut().expect("encoded frame has checksum") ^= 0x01;
    let mut decoder = StreamDecoder::new();
    assert_eq!(decoder.push(&outer), vec![vec![0x66, 0x77]]);
    assert_eq!(decoder.buffered_len(), 0);
}

#[test]
fn rejects_oversized_length_and_continues() {
    let mut input = vec![0xCA, 0xFE];
    input.extend_from_slice(&((MAX_PAYLOAD as u32) + 1).to_be_bytes());
    input.extend_from_slice(&encode(&[0x55]));
    let mut decoder = StreamDecoder::new();
    assert_eq!(decoder.push(&input), vec![vec![0x55]]);
    assert!(decoder.buffered_len() <= MAX_BUFFERED);
}

#[test]
fn decodes_big_endian_length() {
    let payload = vec![0x5A; 258];
    let mut decoder = StreamDecoder::new();
    assert_eq!(decoder.push(&encode(&payload)), vec![payload]);
}

#[test]
fn accepts_the_maximum_payload() {
    let payload = vec![0xA5; MAX_PAYLOAD];
    let mut decoder = StreamDecoder::new();
    let frames = decoder.push(&encode(&payload));
    assert_eq!(frames.len(), 1);
    assert_eq!(frames[0].len(), MAX_PAYLOAD);
    assert_eq!(frames[0][0], 0xA5);
    assert_eq!(frames[0][MAX_PAYLOAD - 1], 0xA5);
    assert_eq!(decoder.buffered_len(), 0);
}

#[test]
fn bounds_buffering_for_large_garbage() {
    let garbage = vec![0x7F; MAX_BUFFERED * 2];
    let mut decoder = StreamDecoder::new();
    assert!(decoder.push(&garbage).is_empty());
    assert_eq!(decoder.buffered_len(), 0);
    assert!(decoder.push(&[0xCA]).is_empty());
    assert_eq!(decoder.buffered_len(), 1);
}

use crate::decoder_api::{Decoder, MAX_PAYLOAD};

const MAGIC: [u8; 2] = [0xCA, 0xFE];
const HEADER_LEN: usize = 6;
const CHECKSUM_LEN: usize = 1;

pub struct StreamDecoder {
    buffer: Vec<u8>,
}

impl StreamDecoder {
    fn discard_invalid_candidate(&mut self) {
        let next_magic = self.buffer[1..]
            .windows(MAGIC.len())
            .position(|window| window == MAGIC)
            .map(|index| index + 1);
        if let Some(index) = next_magic {
            self.buffer.drain(..index);
            return;
        }

        let keep_prefix = self.buffer.last() == Some(&MAGIC[0]);
        self.buffer.clear();
        if keep_prefix {
            self.buffer.push(MAGIC[0]);
        }
    }

    fn process_buffer(&mut self, frames: &mut Vec<Vec<u8>>) {
        loop {
            if self.buffer.is_empty() {
                return;
            }
            if self.buffer[0] != MAGIC[0] {
                self.discard_invalid_candidate();
                continue;
            }
            if self.buffer.len() < MAGIC.len() {
                return;
            }
            if self.buffer[1] != MAGIC[1] {
                self.discard_invalid_candidate();
                continue;
            }
            if self.buffer.len() < HEADER_LEN {
                return;
            }

            let payload_len = u32::from_be_bytes([
                self.buffer[2],
                self.buffer[3],
                self.buffer[4],
                self.buffer[5],
            ]) as usize;
            if payload_len > MAX_PAYLOAD {
                self.discard_invalid_candidate();
                continue;
            }

            let frame_len = HEADER_LEN + payload_len + CHECKSUM_LEN;
            if self.buffer.len() < frame_len {
                return;
            }
            let expected_checksum = self.buffer[HEADER_LEN..HEADER_LEN + payload_len]
                .iter()
                .copied()
                .fold(0u8, |checksum, byte| checksum ^ byte);
            let actual_checksum = self.buffer[frame_len - 1];
            if expected_checksum == actual_checksum {
                frames.push(
                    self.buffer[HEADER_LEN..HEADER_LEN + payload_len].to_vec(),
                );
                self.buffer.drain(..frame_len);
            } else {
                self.discard_invalid_candidate();
            }
        }
    }
}

impl Decoder for StreamDecoder {
    fn new() -> Self {
        Self {
            buffer: Vec::with_capacity(HEADER_LEN),
        }
    }

    fn push(&mut self, chunk: &[u8]) -> Vec<Vec<u8>> {
        let mut frames = Vec::new();
        for byte in chunk.iter().copied() {
            self.buffer.push(byte);
            self.process_buffer(&mut frames);
        }
        frames
    }

    fn buffered_len(&self) -> usize {
        self.buffer.len()
    }
}

impl Default for StreamDecoder {
    fn default() -> Self {
        Self::new()
    }
}

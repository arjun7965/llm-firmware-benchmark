pub const MAX_PAYLOAD: usize = 1024 * 1024;
pub const FRAME_OVERHEAD: usize = 2 + 4 + 1;
pub const MAX_BUFFERED: usize = MAX_PAYLOAD + FRAME_OVERHEAD;

pub trait Decoder: Sized {
    fn new() -> Self;

    fn push(&mut self, chunk: &[u8]) -> Vec<Vec<u8>>;

    fn buffered_len(&self) -> usize;
}

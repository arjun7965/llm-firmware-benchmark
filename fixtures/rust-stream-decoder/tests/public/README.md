# Public Tests

The tests cover arbitrary chunk boundaries, multiple and empty frames,
big-endian lengths, checksum and oversized-frame recovery, partial magic,
maximum payloads, and bounded buffering under large garbage input. The final
test emits the supervisor's randomized completion token.

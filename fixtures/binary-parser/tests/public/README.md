# Public Tests

`test_binary_parser.c` covers empty and maximum payloads, unaligned input, every
truncation boundary, trailing bytes, oversized lengths, bad magic/version/CRC,
null arguments, and output reset on failure.

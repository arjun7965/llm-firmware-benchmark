# Public Tests

`test_firmware_state_machine.c` covers the 1000 ms polling interval, successful
sample publication, failed and rejected transfers, 10 ms retry backoff,
three-attempt timeout exhaustion, overlap prevention, and `uint32_t`
wraparound. A failed later cycle must preserve the latest valid sample.

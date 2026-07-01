# Mocks

`mock_hal.c` provides deterministic time and queued transfer plans. Tests
control start acceptance, completion delay, success, returned bytes, and
`uint32_t` wraparound without wall-clock delays.

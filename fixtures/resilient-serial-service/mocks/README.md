# POSIX Mock

`redirect_posix.h` redirects the candidate's device, polling, signal, pipe, and
termios calls to `mock_posix_serial.c`. The mock scripts device disconnects,
signals, transient errors, backoff timeouts, and cleanup without requiring a
physical serial device.

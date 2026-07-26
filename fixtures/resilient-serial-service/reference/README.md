# Trusted Reference

The trusted implementation uses one nonblocking self-pipe, async-signal-safe
SIGINT/SIGTERM publication, one bounded read per readiness event, and
100-to-1600 ms reconnect backoff that resets after configuration succeeds.

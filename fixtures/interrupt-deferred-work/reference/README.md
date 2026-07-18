# Trusted Reference

The reference uses a release atomic fetch-or in each ISR and an acquire-release
atomic exchange in the foreground. Source bits intentionally coalesce: repeated
interrupts for an already pending source request one later foreground service,
not an unbounded queue entry.

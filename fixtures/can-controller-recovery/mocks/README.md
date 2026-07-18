# Mock CAN0 Model

The fixture-owned mock exposes an opaque CAN0 peripheral. It records every
accessor operation, provides one deterministic RX mailbox, latches TX terminal
and bus-off status, and models global interrupt save/restore. Tests use only
the mock helper API; candidate implementations must use the starter accessors.

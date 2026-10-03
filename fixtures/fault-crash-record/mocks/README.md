# Fault Crash-Record Mocks

The mock records fault-controller access ordering, retained fault status, and
the exact interrupt state used by foreground record operations.

Raw status can include reserved bits; acknowledgements may contain only defined
bits. A watched caller-owned record is copied at each accessor boundary to
observe containment before capture, record completion before acknowledgement,
and zeroing before NORMAL. This models completed-call reset boundaries and
serialized handlers, not torn stores, power-loss durability, or exclusion of
actual non-maskable Cortex-M faults by an interrupt mask.

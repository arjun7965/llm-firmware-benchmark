# Watchdog-Window Recovery Mock

The mock supplies opaque WDT0 configuration, counter, reset-cause, and feed
accessors. Advancing the counter to the configured timeout deterministically
latches a watchdog reset, disables WDT0, and records all accessor ordering and
interrupt-mask values.

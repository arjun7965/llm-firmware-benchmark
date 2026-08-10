# Public tests

Tests cover safe-first initialization, exact four-region permissions and
priority, barriers, readback, pre- and post-enable fault injection, invalid
calls, simultaneous runtime fault bits, containment-before-clear ordering,
latched no-recovery behavior, exact interrupt-state restoration, and combined
readback-fault cases that sample and contain fault bits before any clear. Fault
injection covers every program and readback boundary, and independent base,
size, priority, permission, and execute-never corruptions prove complete
readback comparison. Tests also observe invalid accessor attempts, state
publication before the first security write, stored query flags, and exactly
one configuration-containment call per clean mismatch.

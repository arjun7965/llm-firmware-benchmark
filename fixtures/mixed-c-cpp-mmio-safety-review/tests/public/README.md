# Public Tests

The tests validate fixed-line findings, checked conversion boundaries, opaque
MMIO call order, terminal recovery, idempotent cleanup, move safety, self-move,
independent peripheral instances, and invalid-argument precedence over busy.
They exercise per-object ownership only; callers remain responsible for not
creating simultaneous active owners of one opaque MMIO handle.

# Public Tests

The public tests cover corrupt-journal repair, strict version advancement,
chunk ordering, verifier rejection, interrupted-update fallback, one-boot
trials, confirmation, rollback, and exact interrupt-state restoration.

Eight test groups include individually corrupted journal fields with recomputed
checksums, reverse-slot updates at maximum version/chunk bounds, pending-event
start rejection, event consumption, failed boot re-verification, and journal
snapshots at flash-operation boundaries. Twenty compile-valid mutations verify
that the tests detect the catalogued defects.

# Low-Power Wake/Clock Public Tests

The public tests cover initialization ordering, source/mode validation, stale
latch clearing, idle and deep clock transitions, wake filtering and priority,
and exact interrupt-state restoration. Eleven test groups include an exhaustive
240-case legal mode/mask/status/IRQ matrix, unusable state, rejected-call
preservation, reinitialization, repeated transitions, pure queries, and later
latch arrival. Manager snapshots enforce publication after hardware programming
and before IRQ restoration. All 42 controlled mutations must compile and fail.

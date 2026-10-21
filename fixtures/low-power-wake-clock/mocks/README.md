# Low-Power Wake/Clock Mock

The mock owns the opaque PWRCLK0 state, wake latches, clock/sleep selectors,
and interrupt-mask history. It records every permitted accessor operation for
the deterministic public tests, snapshots the watched manager and IRQ state at
each accessor, and can inject a distinct wake bit immediately after a status
snapshot. These observations check state publication boundaries and one-snapshot
wake decisions without modeling actual IRQ preemption or physical sleep.

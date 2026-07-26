# Low-Power Wake/Clock Mock

The mock owns the opaque PWRCLK0 state, wake latches, clock/sleep selectors,
and interrupt-mask history. It records every permitted accessor operation for
the deterministic public tests.

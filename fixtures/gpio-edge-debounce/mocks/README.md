# GPIO Edge/Debounce Mock

The mock keeps GPIO0 opaque, records every permitted accessor call, models an
active-low button, and latches configured rising/falling edges independently
of normal IRQ delivery. A wake-enabled edge additionally latches wake status.
Foreground interrupt guards are recorded so tests can require exact restore of
the prior global state.

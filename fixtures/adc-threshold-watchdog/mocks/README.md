# ADC Threshold/Watchdog Mock

The mock supplies an opaque ADC0 model with independent conversion-complete,
analog-watchdog, and overrun status latches. It records accessor ordering and
the exact interrupt-mask value so public tests can enforce deterministic
recovery without direct register access.

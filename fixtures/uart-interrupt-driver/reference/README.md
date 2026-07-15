# Trusted UART Driver Reference

The reference treats each UART IRQ as bounded work: it snapshots status once,
handles at most one receive byte and one transmit byte, acknowledges observed
errors, and never spins. Foreground queue operations preserve the caller's
interrupt-mask state around shared driver fields.

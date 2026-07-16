# Trusted GPIO Edge/Debounce Reference

The trusted implementation owns no storage beyond the caller-owned driver
state. It captures one edge in the ISR, lets foreground code establish a
stable active-low level after a wrap-safe 20 ms interval, and preserves edges
that arrive while normal IRQ delivery is temporarily disabled.

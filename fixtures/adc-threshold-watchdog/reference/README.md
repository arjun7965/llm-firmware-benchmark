# Trusted ADC Threshold/Watchdog Reference

This implementation is validator-owned. It configures a 12-bit threshold
window, publishes one terminal sample event per conversion, and requires an
explicit deterministic configuration reset after an overrun or timeout.

# Brownout Safe-Mode Starter

Implement `brownout_safe_mode.h` using only the opaque PWR0 accessors in
`fixture_brownout_safe_mode.h`. The caller owns both manager and retained
state; do not access PWR0 fields directly.

# Trusted reference

The reference uses only the opaque MPU0 and security-controller accessors. It
locks debug/update access and seals secrets before checking status, programs
the four Cortex-M3 regions in priority order, verifies every readback, and
never marks the system ready after a mismatch or fault. Runtime status is
sampled once, contained before the exact snapshot is cleared, and remains
latched until reboot/reinitialization.

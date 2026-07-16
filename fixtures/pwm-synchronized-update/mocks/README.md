# PWM Synchronized-Update Mock

The mock supplies opaque PWM0 shadow and active registers. A load written
while PWM is disabled applies immediately for safe initialization; a load
written while it is enabled applies only when the test advances a period
boundary. It records accessor ordering and the exact interrupt-mask value.

# MPU0 mock boundary

The mock stores deterministic region readback and event traces without
exposing register layouts. It can inject pre-existing, programming-time,
readback-time, enable-time, and enable-readback faults, plus region read
failures. It records explicit configuration containment, fault containment,
clear ordering, barriers, and exact interrupt save/restore values.

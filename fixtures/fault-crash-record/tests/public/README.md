# Fault Crash-Record Public Tests

Public tests cover valid retained records, corruption rejection, hard-fault
capture ordering, explicit recovery gating, and exact interrupt restoration.

Ten groups also exercise isolated corruption of all retained fields, consistent
checksums with invalid magic, repeated reboots, reserved and zero raw status,
sequence wraparound/restart, pending event replacement, unchanged output on
read failure, failed clear preservation, accessor-boundary record snapshots,
and already-disabled IRQ state. Successful read checks start with distinct
sentinels in every output field so omitted copies cannot inherit expected values.
The mutation catalog contains 36 compile-valid
defects spanning these observable contracts.

# Host MMIO Mock

The mock records accessor writes and models the interrupt-clear register's
write-one-to-clear effect. It does not emulate counting or elapsed time; the
task validates configuration and interrupt register behavior only.

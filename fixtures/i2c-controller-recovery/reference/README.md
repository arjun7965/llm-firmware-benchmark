# Trusted I2C Controller Recovery Reference

The trusted reference is validator-owned. It implements one bounded,
asynchronous I2C write state machine, releases the bus on ordinary terminal
paths, avoids STOP after arbitration loss, and uses wrap-safe timeout arithmetic.

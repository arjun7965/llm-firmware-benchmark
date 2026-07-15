# I2C Controller Recovery Mocks

The mock owns an opaque I2C0 register bank and exposes deterministic status
signals for START, address acknowledgement, data acknowledgement, NACK,
arbitration loss, and bus error. It records accessor ordering, clears
write-one-to-clear status bits, and rejects incorrect opaque-register access.

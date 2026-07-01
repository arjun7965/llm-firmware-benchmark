# Starter Assets

`firmware_state_machine.h` defines the required task API and sample structure.
`fixture_hal.h` defines the supplied I2C and millisecond-clock boundary.

The HAL mock guarantees that a read unfinished after 25 ms is no longer active.
Completion remains observable through `i2c_done()` and `i2c_ok()` until the next
start. These semantics must be included when the benchmark prompt adopts this
fixture interface.

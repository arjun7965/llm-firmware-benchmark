# I2C Controller Recovery Starter Contract

`i2c_controller.h` is the complete candidate API. The caller owns the
controller and payload storage until a terminal result is taken. The controller
accepts only one bounded write at a time and does not retry automatically.

`fixture_i2c_controller.h` declares the opaque I2C0 boundary. Candidates must
use only its accessors; the mock records every control, status-clear, data, and
status-read operation.

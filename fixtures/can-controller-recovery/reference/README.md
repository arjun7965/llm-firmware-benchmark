# Trusted Reference

`can_controller.c` is the fixture oracle. It configures a fictional 500 kbit/s
classic-CAN controller, maps RX/TX IRQ events to bounded state, disables the
controller on bus-off, and reconfigures it only after the explicit
recovery-ready indication.

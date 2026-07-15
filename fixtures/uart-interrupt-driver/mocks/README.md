# UART0 Mock

`mock_uart.c` supplies an accessor-instrumented fictional UART0 register bank,
a bounded receive FIFO, an immediate transmit log, write-one-to-clear error
status, and a deterministic interrupt-mask model. Tests explicitly invoke the
non-nested UART ISR, so no host threads or real serial device are required.

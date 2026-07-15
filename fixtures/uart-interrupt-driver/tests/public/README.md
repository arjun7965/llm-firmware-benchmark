# Public UART Driver Tests

The tests disclose exact UART0 initialization, ISR work bounds, transmit and
receive queue behavior, error acknowledgement, overflow accounting, and the
foreground interrupt-mask contract. They do not use host threads or a real
serial device.

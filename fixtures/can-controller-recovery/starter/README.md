# CAN Controller Recovery Starter Contract

Implement `can_controller.h` as one freestanding C11 translation unit. The
opaque `fixture_can_controller.h` API is the only permitted CAN0 and
interrupt-mask interface.

The controller owns one TX request and one software RX slot. It accepts only
11-bit classic-CAN frames with at most eight bytes, uses a non-nested IRQ for
terminal TX/RX/bus-off events, and requires foreground calls to preserve the
exact interrupt state returned by `can0_irq_save_disable`.

# Deterministic Interrupt-Latch Mock

The mock records volatile latch accesses and exact interrupt-mask restoration.
It can invoke a test callback from a status read so a high-priority IRQ can
preempt a low-priority IRQ at a deterministic point.

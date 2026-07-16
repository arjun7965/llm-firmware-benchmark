# Trusted Timer-DMA Handoff Reference

The validator-owned implementation gives DMA exclusive ownership of the timer
compare stream until a terminal IRQ. Aborts and errors retain the last active
compare value and require result acknowledgement plus deterministic recovery
before another DMA handoff.

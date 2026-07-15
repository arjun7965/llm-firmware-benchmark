# Trusted SPI DMA Reference

The reference programs RX DMA before TX DMA, records each transfer's ownership
in caller-provided state, and exposes completion only after both channels
finish. The non-nested DMA interrupt snapshots and acknowledges status once,
then either performs terminal teardown on an error or waits for the matching
channel completion.

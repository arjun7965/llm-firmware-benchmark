# Deterministic Cache and DMA Mock

The mock records cache clean/invalidate and DMA operations in order. Tests can
choose the next transmit-start, receive-start, or receive-finish status without
using hardware, host threads, or an actual cache.

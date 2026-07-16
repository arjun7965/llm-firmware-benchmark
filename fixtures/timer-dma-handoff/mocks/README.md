# Timer-DMA Handoff Mock

The mock keeps timer and DMA registers opaque, applies one bounded compare
sample per deterministic timer tick, records every accessor call, and latches
DMA completion, abort, and error status without host threads.

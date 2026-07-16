# Timer-DMA Handoff Starter Contract

Implement `timer_dma_handoff.h` using only the opaque accessors declared by
`fixture_timer_dma_handoff.h`. The caller owns `timer_dma_t` and keeps a
started sample sequence valid until the driver returns a terminal result.

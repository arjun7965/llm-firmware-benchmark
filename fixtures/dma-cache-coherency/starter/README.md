# DMA Cache-Coherency Starter API

Implement `dma_cache_transfer.h` against the opaque cache and DMA boundary in
`fixture_dma_cache.h`. The fixture models one non-coherent Cortex-M7 DMA
channel, 32-byte cache lines, four-byte DMA-buffer alignment, and one pending
receive transfer.

The cache helpers record ranges but never dereference them. Candidate code must
round the affected cache range down and up without changing the original DMA
buffer pointer or length.

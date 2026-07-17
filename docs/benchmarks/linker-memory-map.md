# Linker Symbol Memory-Map Initialization

## Objective

Assess reset-time validation and use of a fixed linker-symbol memory map:
initialized-data copying from flash to SRAM, `.bss` clearing, and cached image,
writable, and stack boundary queries.

Implement the API declared by
`fixtures/linker-memory-map/starter/linker_memory_map.h` using only the
supplied opaque linker-symbol and byte-transfer accessors.

## Target Assumptions

Target profile: `armv7m-bare-metal`. The fixture models a single-core
little-endian Cortex-M3 with AAPCS/EABI and fixed flash/SRAM ranges. Reset-time
code starts with interrupts masked and runs before application concurrency. The
linker layout is represented by opaque accessors so host validation can record
symbol reads and memory transfers without a real board or linker script. There
is no heap, cache, DMA, FPU, RTOS, vendor SDK, or host thread.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Reads the complete linker contract,
  copies initialized data to the correct SRAM range, clears `.bss`, and
  publishes the expected map bounds.
- 1 point — **Bounded resource use:** Uses caller-owned state and bounded
  section-length loops without allocation, polling, retry, or global state.
- 1 point — **Timing behavior:** Preserves the required startup order: all
  symbols first, forward data copy, BSS clear, then state publication.
- 1 point — **Concurrency safety:** Respects the reset-time single-core
  contract and never exposes initialized state before memory initialization is
  complete.
- 2 points — **Fault recovery:** Rejects malformed, overlapping, unaligned,
  or out-of-range linker symbols without changing SRAM or prior map state.
- 2 points — **Portability:** Uses fixed-width arithmetic, half-open ranges,
  and supplied opaque accessors without direct linker-pointer or memory access.
- 1 point — **Clarity and validation:** Explains the linker-range rules,
  startup transfer ordering, cached range queries, and deterministic tests.

Treating an image outside flash, allowing BSS to overlap the stack, copying a
mismatched data range, or including exclusive end addresses are substantial
memory-map defects.

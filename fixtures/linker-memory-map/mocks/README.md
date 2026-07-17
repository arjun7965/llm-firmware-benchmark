# Linker Memory-Map Mock

The mock exposes linker-symbol values plus byte-addressed flash and SRAM through
opaque accessors. It records every symbol read and memory transfer so public
tests can validate startup ordering without a board or linker script.

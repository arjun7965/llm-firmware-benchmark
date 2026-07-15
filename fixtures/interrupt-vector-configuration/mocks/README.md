# Mock Hardware Boundary

The deterministic mock owns opaque SCB and NVIC state, linker-address mapping,
vector-entry writes, synchronization barriers, and the global interrupt state.
It records ordering so public tests can reject unsafe startup relocation and
runtime vector updates without executing a real exception handler.

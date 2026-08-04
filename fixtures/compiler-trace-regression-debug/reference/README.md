# Trusted Reference

`compiler_trace_debug.c` records the single cross-evidence diagnosis and keeps
all transfer arithmetic in `uint32_t`. It validates caller-owned state before
planning one bounded chunk and never touches DMA hardware.

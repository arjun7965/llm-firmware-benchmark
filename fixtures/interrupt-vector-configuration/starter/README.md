# Starter Interface

`interrupt_vector.h` declares the single-file C11 implementation contract.
`fixture_vector_table.h` supplies the Cortex-M3 vector layout constants, opaque
SCB/NVIC accessors, linker-address translation, and interrupt-mask boundary.
Only those accessors may touch the vector table or hardware model.

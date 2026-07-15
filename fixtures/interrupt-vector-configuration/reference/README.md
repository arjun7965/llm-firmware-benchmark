# Trusted Reference

The reference initializes a linker-reserved RAM vector table before publishing
its address to VTOR, clears all modeled external interrupt state, and protects
live vector/enable updates with the supplied interrupt-save boundary.

# Trusted Reference

The trusted implementation validates the fixed linker contract before copying
the initialized-data range, zeroing `.bss`, and publishing caller-owned map
state.

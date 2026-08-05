# MMIO Mock

The C mock implements the opaque accessor ABI for independent register instances.
It records every accessor call so public tests can verify ordering without direct
register overlay or host-layout assumptions.

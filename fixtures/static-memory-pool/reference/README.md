# Trusted Reference

The trusted implementation uses the pool's embedded storage and a fixed
four-entry allocation map. It never allocates dynamically, chooses the lowest
available block, validates releases by integer address offset, and leaves the
allocation state unchanged on an invalid release.

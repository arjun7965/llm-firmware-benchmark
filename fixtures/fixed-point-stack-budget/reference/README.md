# Trusted Reference

The reference uses scalar 64-bit intermediates, explicit signed Q1.15
rounding, saturation, and no heap, recursion, or variable-length allocation.
It scans the caller-owned watermark before processing so an over-budget stack
cannot change output.

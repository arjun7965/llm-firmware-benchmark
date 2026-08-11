# Trusted Reference

The reference exploits coefficient symmetry to combine paired Q1.15 samples
before four validator-instrumented 64-bit MACs. It rounds once, saturates, and
updates history only after the 28-cycle cost transaction commits. This produces
the same integer result as the full seven-tap convolution while using three
fewer modeled MACs.

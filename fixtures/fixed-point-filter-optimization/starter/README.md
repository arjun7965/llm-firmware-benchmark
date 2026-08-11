# Fixed-Point Filter Optimization Starter API

Implement `fixed_point_filter.h` in one C11 source file. The filter is a causal
seven-tap symmetric Q1.15 FIR. History index zero is the previous input and
index five is the input delayed by six steps. Successful initialization starts
from six zero-valued prior samples.

The supplied cost boundary models twelve fixed scalar cycles plus four cycles
per Q1.15 MAC. A step must declare four MACs, use the MAC accessor for the three
symmetric pairs and center tap, and commit the cost transaction before
publishing state or output. Direct seven-tap multiplication exceeds the
28-cycle budget. The exact dyadic coefficients and nearest rounding keep error
within one Q1.15 output LSB.

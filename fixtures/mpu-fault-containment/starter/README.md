# MPU fault containment

Implement the API in `mpu_fault_containment.h` as one freestanding C11 answer.
MPU0 and the security controller are opaque; use only the supplied accessors.
The four regions must be programmed in the listed order, with the stack guard
highest priority. Initialization is safe-first and enters normal mode only
after exact readback and a second clean fault sample. A runtime IRQ is
irrecoverable until reboot/reinitialization. Fault injection here is a
deterministic latch/containment model, not a claim of physical resistance to
DMA or laboratory attacks.

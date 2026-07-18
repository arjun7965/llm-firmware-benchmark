# Deterministic RTOS Queue Mock

The mock supplies one fixed-size FIFO queue and one matching counting semaphore.
It records call order and bounded timeout arguments, preserves sample order,
and can inject individual API failures without threads or wall-clock timing.

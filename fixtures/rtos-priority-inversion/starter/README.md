# RTOS Priority-Inversion Starter API

`priority_inversion.h` is the complete answer boundary. It defines a
caller-owned priority guard and the low-priority telemetry and high-priority
safety acquisition entry points.

`fixture_rtos.h` supplies the only RTOS API available to the answer. The mock
models priority inheritance when its mutex creation flag is true; answer code
must not depend on mock-only headers.

# RTOS Queue and Semaphore Starter API

`queue_semaphore.h` defines a caller-owned sensor pipeline with a four-item
queue and matching counting-semaphore tokens. The producer must publish a
sample before making its token visible; the worker has a bounded wait.

`fixture_rtos_queue.h` provides the complete opaque RTOS boundary. Do not use
mock-only headers, vendor APIs, dynamic allocation, or a polling loop.

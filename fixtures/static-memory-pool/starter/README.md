# Static Memory Pool Starter API

Implement `static_memory_pool.h` in one C11 source file. The pool's storage is
embedded in the caller-owned `static_memory_pool_t`; no heap API is available or
needed. Every allocated block must be 16-byte aligned and exactly 32 bytes.

The public tests own the object lifetime and verify deterministic first-free
allocation, exhaustion, reuse, invalid-pointer rejection, and reinitialization.

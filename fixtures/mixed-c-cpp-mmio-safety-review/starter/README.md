# Mixed C/C++ MMIO Safety Review Starter

Return one fenced C++17 implementation that includes `mmio_safety_review.hpp`.
The supplied C ABI deliberately exposes only opaque volatile register handles and
accessor functions. The evidence files are immutable review input, not headers
to compile into the answer. The required findings describe MISRA-style concerns;
they do not claim formal MISRA compliance.

One `mmio_transfer_t` object rejects a second start while it is active. The
caller must ensure no other live active owner uses that same opaque MMIO handle;
without mutable global state, the implementation cannot maintain a cross-object
handle registry. Move assignment between two different active objects requires
distinct handles; self-move remains safe.

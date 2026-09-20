# Public Tests

The tests cover invalid inputs, exact and crossing cache-line boundaries,
maximum transfer length, buffer-end and cache-rounding overflow, the last
representable cache range, clean/invalidate ordering, start-status propagation,
busy receive-state preservation, terminal error cleanup, independent TX during
RX, and reinitialization of a pending receive. Synthetic high addresses are
recorded by the opaque mock and never dereferenced.

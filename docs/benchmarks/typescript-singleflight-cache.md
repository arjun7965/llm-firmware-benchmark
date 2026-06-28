# TypeScript Singleflight Cache

## Objective

Assess asynchronous coalescing, caller-local cancellation, and invalidation
races in a generic TypeScript cache.

## Scoring

- 3 points — One load per key is shared and stale in-flight loads cannot repopulate invalidated data.
- 2 points — A caller can cancel its wait without cancelling work required by others.
- 2 points — TTL, loader rejection, synchronous throws, invalidate, and clear behave consistently.
- 2 points — Deterministic fake-clock tests cover coalescing and the critical races.
- 1 point — Types, cleanup, and explanation are production-quality without per-entry timers.

Deleting only the cached value without versioning the in-flight load does not
satisfy invalidation semantics.

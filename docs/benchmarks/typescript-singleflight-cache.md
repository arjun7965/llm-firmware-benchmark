# TypeScript Singleflight Cache

## Objective

Assess asynchronous coalescing, caller-local cancellation, and invalidation
races in a generic TypeScript cache.

The scaffold contract is under `fixtures/typescript-singleflight-cache/`.
Candidates export one generic cache module; validator-owned tests use an
injected fake clock and deferred promises to control every completion order.

## Scoring

- 3 points — One load per key is shared and stale in-flight loads cannot repopulate invalidated data.
- 2 points — A caller can cancel its wait without cancelling work required by others.
- 2 points — TTL, loader rejection, synchronous throws, invalidate, and clear behave consistently.
- 2 points — Deterministic fake-clock tests cover coalescing and the critical races.
- 1 point — Types, cleanup, and explanation are production-quality without per-entry timers.

Deleting only the cached value without versioning the in-flight load does not
satisfy invalidation semantics.

The trusted reference and eighteen controlled mutations are calibrated with:

```bash
npm run fixture:typescript-cache:self-test
```

The fixture remains a scaffold until the exact profile dependency set is
attested, mounted, and executed inside the sandbox namespace.

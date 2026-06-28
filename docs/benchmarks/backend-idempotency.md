# Backend Idempotency

## Objective

Assess database-backed idempotency under concurrent requests and process
failures without relying on process-local coordination.

## Scoring

- 3 points — Schema constraints and transaction boundaries guarantee one atomic order.
- 2 points — Same-payload retries replay success and different payloads return 409.
- 2 points — Concurrent and in-progress duplicates have a coherent blocking strategy.
- 2 points — Authentication scope, canonical request identity, and failures are handled.
- 1 point — SQL and TypeScript are clear enough to implement and review.

An application-only check followed by an insert is not concurrency-safe and
cannot receive full credit for the first criterion.

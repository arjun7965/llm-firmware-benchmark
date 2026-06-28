# Webhook Replay Security

## Objective

Assess cryptographic request authentication, distributed replay prevention, and
atomic event processing.

## Scoring

- 3 points — HMAC verification uses exact raw bytes, constant-time comparison, and safe rotation.
- 2 points — Database constraints prevent cross-instance replay and detect conflicting ID reuse.
- 2 points — Event persistence and outbox enqueue commit atomically with idempotent success.
- 2 points — Timestamp windows, authentication ordering, failures, and security tests are complete.
- 1 point — Schema and Express/pg code are coherent and implementation-ready.

Parsing or reserializing JSON before signature verification loses raw-body
authentication credit.

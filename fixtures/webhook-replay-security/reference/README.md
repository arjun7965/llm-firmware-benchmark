# Trusted webhook replay security reference

The handler captures the request as raw bytes, verifies every configured HMAC
secret before parsing JSON, and binds the webhook ID to a digest of the signed
timestamp plus exact body bytes. The migration makes that ID globally unique
and makes each accepted event own exactly one outbox row inside the same
transaction.

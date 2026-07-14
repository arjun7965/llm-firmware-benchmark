# Trusted backend idempotency reference

The SQL migration owns the public table contract. The Express factory performs
the request validation and PostgreSQL transaction protocol. Its unique insert
claims the key; a conflicting request locks and examines the committed record
before returning a replay, a conflict, or an in-progress response.

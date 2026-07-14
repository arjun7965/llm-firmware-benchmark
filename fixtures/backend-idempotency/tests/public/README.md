# Public backend idempotency tests

The suite starts the candidate Express application on a private Unix socket
inside the candidate namespace. It verifies replay identity, concurrent
duplicate suppression, raw-byte request binding, user scoping, processing
records, input validation, the supplied authentication middleware, atomic
rollback, and the non-superuser PostgreSQL connection.

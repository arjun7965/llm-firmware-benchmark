# Public webhook replay security tests

The public suite validates raw-byte HMAC authentication, five-minute timestamp
windows, secret rotation, duplicate behavior across two application instances,
conflicting delivery IDs, one outbox action per event, and rollback on an
outbox failure. It also rejects parsing malformed JSON before authentication.

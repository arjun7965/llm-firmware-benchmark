# Public tests

Tests cover exact unaligned little-endian frames, malformed input, policy
gates, independent replay domains, post-authentication sequence commits,
challenge expiry and one-time use, lockout, update-version/signature policy,
life-cycle denial, and invalid API calls. Tests begin from open mock gates so
initialization writes are observable, isolate each malformed field behind an
otherwise eligible challenge, and prove that every denial relocks both gates,
revokes published update authorization, and clears software authorization
state. Lifecycle and physical-presence reads, generic expiry denial, invalid
accessor attempts, and initialization state publication are independently
observable. Event logs prove exact verifier arguments and ordering at the
opaque SEC0 boundary.

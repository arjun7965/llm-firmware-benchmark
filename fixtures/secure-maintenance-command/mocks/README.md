# SEC0 mock boundary

The mock records accessor calls and supplies deterministic lifecycle, presence,
challenge, and immutable verifier verdicts. The configured verdict is returned
unchanged; tags and digests are only recorded as verifier arguments. It records
distinct debug/update gate and authorization-revocation events plus exact
verifier and publication arguments.
It intentionally has no key accessor and does not expose any secret material to
the candidate.

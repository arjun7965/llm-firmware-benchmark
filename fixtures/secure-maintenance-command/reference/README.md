# Trusted reference

The reference treats SEC0 as an immutable cryptographic and lifecycle
boundary. It decodes both exact wire formats from bytes, performs structural
and policy checks before one verifier call, and commits replay state only
after an affirmative verdict. Starting a debug challenge also locks the
update gate and revokes any prior update authorization. It contains no key
bytes or cryptographic implementation.

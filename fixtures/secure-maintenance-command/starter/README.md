# Secure maintenance command

Implement the API in `secure_maintenance_command.h` as one freestanding C11
answer. The only security boundary is the opaque SEC0 interface in
`fixture_secure_maintenance.h`; never inspect, cast, copy, or expose its
secret. The two wire formats are deliberately byte-oriented and must be
decoded little-endian without structure casts. `SEC0_MAX_CHALLENGE_TTL` is
less than half the 32-bit clock space, so deadline comparisons must be
wrap-safe. A debug unlock is not update authorization. Beginning a new
challenge locks both gates and revokes any earlier update authorization.

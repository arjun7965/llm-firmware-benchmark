# Secure Maintenance Command

## Objective

Assess a boot/recovery maintenance interface that receives attacker-controlled
unaligned bytes. The implementation must keep SEC0 lifecycle and cryptographic
secrets opaque while separating debug unlock from update authorization.

Implement `fixtures/secure-maintenance-command/starter/secure_maintenance_command.h`.
The frozen prompt is the complete answer contract; the SEC0 mock is not a
cryptographic implementation and exposes no key accessor.

## Target assumptions

Target profile: `armv7m-bare-metal`. The boot stage is single-core,
little-endian Cortex-M3/AAPCS. Frames are exactly 16 or 24 bytes and all
fields are decoded explicitly as little-endian bytes. Challenge TTLs use a
32-bit wrap-safe half-range comparison. Lifecycle, physical presence,
verifier verdicts, and gate writes are immutable fixture boundaries; SEC0
returns its configured verifier verdict unchanged, with tags and digests only
serving as verifier arguments. Update authorization is distinct from secure-boot
image admission. Issuing a new debug challenge locks both gates and revokes any
prior update authorization.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Exact framing, field validation, policy,
  replay, challenge, lockout, version, and authorization results are correct.
- 1 point — **Bounded resource use:** Caller-owned state, no heap, polling, retries,
  mutable globals, or unbounded work.
- 1 point — **Timing behavior:** Deadline handling is wrap-safe and verifier calls are
  bounded and occur only after structural and policy checks.
- 1 point — **Concurrency safety:** State transitions are coherent for the
  non-interleaved boot-stage contract and denial paths do not publish gates.
- 2 points — **Fault recovery:** Initialization and denials fail closed; challenges
  are one-time, lockout is fixed, and failed authentication never advances
  replay state or publishes update authorization; challenge issuance also
  revokes an earlier update authorization.
- 2 points — **Portability:** C11 byte parsing handles unaligned input without
  structure casts, undefined behavior, direct registers, or host layout.
- 1 point — **Clarity and validation:** Explanation identifies the SEC0 secret
  boundary and tests demonstrate exact call ordering and malformed traffic.

The prompt is self-contained: its API, state layout, frame offsets, constants,
SEC0 signatures, minimum-version-zero rule, non-wrapping sequence rule,
challenge TTL/deadline rule, challenge-begin return value, cumulative lockout,
and denial side effects are normative. The public tests and mutations are
authoritative for deterministic rejection.

## Calibration

Run `npm run fixture:secure-maintenance:self-test` to exercise the trusted
reference and its eight public test groups. The C mutation suite rejects all
40 compile-valid controlled defects for framing, replay, policy, verifier,
lockout, authorization-publication, and fail-closed ordering behavior.

# Secure Boot Image Validation

## Objective

Assess secure boot admission of a signed firmware image through a constrained
boot-ROM interface. The implementation must reject malformed, stale, tampered,
or unsigned images before it selects an executable boot target.

Implement fixtures/secure-boot-image-validation/starter/secure_boot_image_validation.h
against the supplied opaque BOOT0 accessors.

## Target Assumptions

Target profile: `armv7m-bare-metal`. A single-core little-endian Cortex-M3 boots
with normal image delivery disabled. Image bytes and trusted key material remain
inside immutable BOOT0; the candidate can read only a copied header, ask BOOT0
to measure an opaque slot, and request its immutable signature verdict. A
successful boot target represents the final transfer boundary rather than a
direct function call.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Validates all header fields, including
  half-open image range and Thumb entry boundaries, enforces the minimum
  version and signature policy, and returns the specified result for each
  rejection.
- 1 point — **Bounded resource use:** Uses caller-owned state and bounded
  accessor calls without allocation, polling, retries, or mutable globals.
- 2 points — **Timing behavior:** Reads the header before policy checks, measures
  only a structurally current candidate, and invokes signature verification
  only after the measured digest matches.
- 1 point — **Concurrency safety:** Treats boot admission as a non-interleaved
  boot-stage operation and leaves invalid or uninitialized attempts without
  shared-state or BOOT0 side effects.
- 2 points — **Fault recovery:** Locks recovery for every rejected image,
  never unlocks normal boot after a failure, keeps raw bytes and trusted keys
  inside BOOT0, and selects a target only after immutable acceptance.
- 1 point — **Portability:** Uses freestanding C11 and only fixture-owned
  BOOT0 accessors, with no direct register access or vendor dependencies.
- 1 point — **Clarity and validation:** Explains validation order, version
  policy, secure-transfer boundary, and deterministic negative tests.

Selecting a slot before signature acceptance, accepting a stale version, or
unlocking recovery after rejection is a substantial security defect.

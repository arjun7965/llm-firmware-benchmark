# Dual-Slot Update Recovery

## Objective

Assess a power-loss-safe dual-slot update lifecycle. The implementation must
keep a known confirmed image bootable while it stages a strictly newer
candidate, grant an accepted candidate one trial boot, and roll it back unless
the application explicitly confirms it.

Implement fixtures/dual-slot-update-recovery/starter/dual_slot_update_recovery.h
against the supplied opaque FLASH0 accessors.

## Target Assumptions

Target profile: `armv7m-bare-metal`. A single-core little-endian Cortex-M3 has
two mutually exclusive flash slots and a caller-owned retained journal. The
factory slot/version comes from immutable boot configuration. Flash writes,
candidate verification, and boot-target selection are observable only through
FLASH0. Boot-stage selection runs before normal delivery; foreground update
transitions preserve the exact caller interrupt state.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Validates journal integrity,
  slot/chunk bounds, strict version advancement, and the defined lifecycle
  results and events.
- 1 point — **Bounded resource use:** Uses caller-owned state and at most one
  bounded sequence of flash operations per transition, with no allocation,
  polling, retry, or global state.
- 2 points — **Timing behavior:** Erases before recording WRITING, records
  chunk progress after each program operation, verifies before TRIAL, and
  records ATTEMPTED before selecting a trial target.
- 1 point — **Concurrency safety:** Foreground transitions save and restore
  the exact interrupt state; event consumption is atomic.
- 2 points — **Fault recovery:** Repairs an invalid journal to the immutable
  factory baseline, preserves confirmed boot through interrupted updates,
  erases rejected candidates, and rolls back an unconfirmed trial.
- 1 point — **Portability:** Uses freestanding C11 and only fixture-owned
  FLASH0 accessors, without direct register access or vendor dependencies.
- 1 point — **Clarity and validation:** Explains journal integrity,
  interruption boundaries, trial confirmation, rollback, and deterministic
  tests.

Erasing the confirmed slot, booting an interrupted candidate, granting repeated
unconfirmed trials, or promoting a non-advancing version is a substantial
update-safety defect.

## Validation Boundary

The prompt supplies both headers and defines usable manager state. Public tests
observe journal snapshots at FLASH0 accessor boundaries, including erase before
WRITING, progress after programming, verification before TRIAL, ATTEMPTED before
trial selection, and idle before rollback erasure. This is a deterministic
journal-transition model; it does not establish byte-level torn-write resilience,
physical flash durability, or cryptographic image authenticity.

## Calibration

The trusted reference passes eight public test groups and all 20 compile-valid
controlled mutations are rejected in the pinned C11 sandbox. The completed
[three-family pilot](../model-family-calibration.md#completed-dual-slot-update-pilot--dual-slot-update-recovery)
records nine attempts with identical task prompts, no retries, and no candidate
repairs. AI criterion scores were frozen before unblinding and subsequently
human-reviewed. Report deterministic pass rates separately from rubric scores:
static credit for a failed extraction or compilation is not executable success.

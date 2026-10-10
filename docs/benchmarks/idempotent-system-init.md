# Idempotent System Initialization

## Objective

Assess reset-time system initialization that is side-effect-free for repeated
same-config calls, fails closed on conflicting reconfiguration, and retains an
explicit safe-mode lifecycle across resets.

Implement `fixtures/idempotent-system-init/starter/idempotent_system_init.h`
against the supplied opaque SYSTEM0 accessors.

## Target Assumptions

Target profile: `armv7m-bare-metal`. A single-core little-endian Cortex-M3
uses caller-zeroed state plus a caller-owned retained record. Boot programming
runs with interrupts already masked; safe-mode, resume, and event operations
restore the caller's exact interrupt state.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Validates configurations/records,
  programs the documented initial sequence, identifies repeat and conflict
  calls, and reports each lifecycle result accurately.
- 1 point — **Bounded resource use:** Uses fixed caller-owned state and bounded
  writes with no allocation, polling, retry, or mutable global state.
- 1 point — **Timing behavior:** Preserves the declared SAFE/clock/mask/READY
  order for initial setup and explicit resume.
- 2 points — **Concurrency safety:** Foreground safe-mode, resume, and event
  operations save/restore exact interrupt state and reject pending transitions.
- 2 points — **Fault recovery:** Repairs corrupt retained state, preserves a
  retained safe boot latch, rejects corrupt-record resume, and requires
  explicit event consumption before recovery.
- 1 point — **Portability:** Uses freestanding C11 and only opaque SYSTEM0
  accessors without direct registers or platform-specific APIs.
- 1 point — **Clarity and validation:** Explains idempotency, conflict policy,
  safe-mode persistence, programming order, and deterministic tests.

Repeating controller writes for an already-ready configuration, accepting a
conflict, or resuming before safe-mode acknowledgement is a substantial safety
defect.

## Calibration Scoring Policy

Freeze criterion scores before opening the identity key. Generation failures
have no rubric score. Extraction or compilation failures receive zero functional
correctness; other criteria may receive supported static credit. A deterministic
pass does not automatically earn ten points. Use half-point partial credit and
assign each defect to its owning dimension without duplicate deductions.

Configuration/result handling, pointer conflicts, signatures, checksum writing,
count arithmetic, state binding, and event values belong to functional
correctness. Unbounded work belongs to resource use; controller SAFE/clock/mask/
READY order belongs to timing; usable-state checks, critical-section placement,
updates before restore, and exact IRQ restoration belong to concurrency.
Retained validity/repair, safe boot preservation, and transition/event gates
belong to fault recovery. Forbidden APIs and language dependencies belong to
portability. Clarity requires an accurate explanation of the modeled persistence
boundary and concrete test scenarios.

## Deterministic Model

The prompt supplies both exact headers, inclusive configuration limits, the
usable-state predicate, and complete accepted/rejected accessor traces.
Fourteen public test groups exercise isolated retained corruption, wrong magic
and out-of-range safe bits with consistent checksums, saturated and repeated
boots, retained safe boot with a different requested signature, config aliasing,
no-effect repeat/conflict paths, explicit event gating, and opaque IRQ values
including zero. Mock snapshots at accessors verify foreground state, record,
and event publication before interrupt restoration. A post-generation audit
also checks complete repair to a valid count-zero record before initial SAFE
and preservation through READY, with boot count increment committed afterward.

Reset and retained corruption occur between completed serialized API calls.
Torn stores, power loss inside calls, physical retained-memory durability,
actual interrupt preemption, and adversarial checksum collisions are excluded.
The XOR checksum detects the exercised corruption; it is not authentication.

The original cohort used thirteen groups and 54 mutations. Static review exposed
an implementation that deferred invalid-record repair until after READY. The
frozen prompt already required repair before programming, so supplemental checks
use the same prompt and unchanged answers. Keep original validation outcomes
separate from the stronger fourteen-group, 55-mutation audit.

## Calibration

The reviewed three-family, three-attempt pilot records all nine answers and
original thirteen-group passes. The stronger fourteen-group audit preserves
those outcomes and rejects Kimi run 1 for deferring invalid-record repair until
after READY. Luna and GLM pass 3/3 stronger checks; Kimi passes 2/3.

Reviewed rubric means are 9.500 for GPT-5.6 Luna, 9.667 for GLM-5.3, and 9.500
for Kimi K3. Scores are AI-assisted, human-reviewed, frozen before unblinding,
and approved without changes on October 9, 2026 (America/Los_Angeles).
The deferred-repair deduction belongs only to fault recovery; explanation
limitations belong only to clarity. Deterministic outcomes remain separate.

See the [methodology and reviewed results](../model-family-calibration.md#completed-idempotent-initialization-pilot--idempotent-system-init)
and [sanitized aggregate](../calibration/idempotent-system-init-2026-10-09.json).

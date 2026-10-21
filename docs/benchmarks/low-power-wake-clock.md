# Low-Power Wake and Clock Transition

## Objective

Assess a bounded Cortex-M power manager that arms explicit wake sources, enters
idle or deep sleep in a safe order, and restores the run clock after a real
wake without mistaking a stale or unconfigured latch for a wake event.

Implement the API declared by
`fixtures/low-power-wake-clock/starter/low_power_wake_clock.h` using only the
supplied opaque PWRCLK0 and interrupt-mask accessors.

## Target Assumptions

Target profile: `armv7m-bare-metal`. The fixture models a single-core
little-endian Cortex-M3 with AAPCS/EABI, no heap, cache, FPU, DMA, RTOS,
vendor SDK, or host threads. PWRCLK0 provides a 48 MHz run clock and a 4 MHz
deep-sleep clock. RTC, GPIO, and UART are independently latched wake sources;
UART is deliberately unavailable in deep sleep. Foreground power transitions
must preserve the caller's exact global interrupt state.

## Scoring

Scoring profile: `firmware-v1`.

- 2 points — **Functional correctness:** Initialization establishes a known awake PWRCLK0 state; valid idle/deep requests retain their exact mask and mode; and resume returns GPIO, RTC, or UART only for an enabled source with the stated priority.
- 1 point — **Bounded resource use:** The implementation uses caller-owned state, one status snapshot per resume, and no allocation, polling loop, retry loop, or unbounded work.
- 2 points — **Timing behavior:** Deep sleep selects the 4 MHz clock only after stale latches are cleared and before entry; every successful wake returns the 48 MHz run clock before normal operation; and the clock-rate query reflects the active sleep state.
- 2 points — **Concurrency safety:** Valid prepare/resume calls save-disable and restore the exact interrupt state once, with wake sources disabled during transition and armed before the sleep mode is selected.
- 1 point — **Fault recovery:** Invalid masks, deep UART requests, uninitialized calls, and duplicate sleep arming have no effects; a raw latch outside the configured mask keeps the system asleep; successful recovery disables configured wakes and clears only observed configured latches.
- 1 point — **Portability:** The answer uses freestanding C11 and only fixture-owned PWRCLK0/interrupt accessors, without direct MMIO fields, pointer casts, inline assembly, or vendor APIs.
- 1 point — **Clarity and validation:** The explanation covers source filtering, GPIO-over-RTC-over-UART priority, stale-latch handling, clock transitions, and deterministic tests for ordering and interrupt restoration.

Do not award clock-transition or recovery credit for retaining stale latches,
allowing UART in deep sleep, entering a sleep mode before its wake mask is
armed, treating an unconfigured raw latch as a wake event, or leaving the
sleep clock selected after resume.

## Calibration Scoring Policy

Freeze criterion scores before opening the identity key. Generation failures
have no rubric score. Extraction or compilation failures receive zero functional
correctness; other criteria may receive supported static credit. A deterministic
pass does not automatically earn ten points. Use half-point partial credit and
assign each defect to its owning dimension without duplicate deductions.

State fields, selected wake reasons, and clock-query values belong to functional
correctness. Unbounded work, allocation, and mutable globals belong to resource
use. Hardware accessor ordering, stale-latch clearing before arming, and clock
selection/restoration belong to timing. Critical sections, exactly one status
snapshot, state publication at the specified boundaries, and exact IRQ restore
belong to concurrency. Unusable/invalid/duplicate call rejection, configured-
source filtering, no-source preservation, and clearing only observed configured
bits belong to fault recovery. Forbidden APIs and language dependencies belong
to portability. Clarity requires an accurate explanation of the modeled
boundary and concrete test scenarios; do not deduct here again for an
implementation defect already assigned elsewhere.

## Deterministic Model

The prompt supplies both exact headers and complete accepted/rejected accessor
traces. A usable manager is non-NULL, initialized, and bound to non-NULL
registers. Eleven public test groups cover initialization and reinitialization,
invalid modes/masks, unusable managers, duplicate prepare, awake resume,
repeated transitions, and pure clock queries. The exhaustive matrix covers 240
legal mode/mask/status/IRQ combinations, including zero and opaque saved IRQ
states. Accessor snapshots check that state stays unchanged through hardware
setup and is committed before interrupt restoration. Controlled post-read
latches check that one snapshot governs the current wake and preserves later
distinct bits.

The mutation catalog contains 42 compile-valid defects spanning these contracts.

Calls are serialized and usable state is caller-preserved. Accessors complete
their non-failing modeled actions on return. Reentrancy, actual IRQ preemption,
physical clock settling, energy, latency, and repeated arrivals on an already-
observed latch bit are outside the model. Initialization is quiescent boot-stage
work; the sleep selector does not execute WFI or suspend host execution.

## Calibration

The fresh three-family pilot recorded all nine scheduled answers: six passed,
two failed compilation, and one failed the duplicate-prepare check. Mean rubric
scores were 9.833/10 for GPT-5.6 Luna (2/3 passes), 8.000 for GLM-5.3 (1/3), and
9.667 for Kimi K3 (3/3). Rubric scores include supported static partial credit
and remain separate from deterministic pass rates.

Criterion scores were frozen before unblinding and approved without changes by
the conversation user on 2026-10-21 (America/Los_Angeles). Scoring is AI-assisted,
human-reviewed, with prior coordinator exposure disclosed; it is not independent
human scoring. The earlier nine-attempt cohort remains unscored diagnostic
evidence after six provider authentication failures. No attempts were replaced
or pooled, and the fresh cohort retained the same prompt and model settings.

See the [methodology and findings](../model-family-calibration.md#completed-low-power-wake-and-clock-pilot--low-power-wake-clock)
and [sanitized aggregate](../calibration/low-power-wake-clock-2026-10-20.json)
for per-run rationale, provider budgets, pinned validation details, and model
limits. The small cohort and unequal generation budgets do not establish a
broad model ranking.

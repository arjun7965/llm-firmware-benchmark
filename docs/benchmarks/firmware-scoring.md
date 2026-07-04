# Firmware Scoring Profile

Profile ID: `firmware-v1`

Use this profile for every task in the `firmware` suite. Choose weights before
collecting model outputs and keep them unchanged across compared runs. Each task
rubric must list all seven dimensions below and total 10 points. A dimension
that is genuinely out of scope may receive 0 points, but the rubric must explain
why; do not invent requirements solely to give every dimension a positive
weight.

## Dimensions

| Dimension | Evidence to evaluate |
| --- | --- |
| Functional correctness | Successful-path behavior, interfaces, state, and output |
| Bounded resource use | Memory, storage, allocation, and buffer-capacity limits |
| Timing behavior | Deadlines, wraparound, latency, lock-freedom, blocking, and busy-waiting |
| Concurrency safety | Ownership, atomics, overlapping operations, interrupt/RTOS interaction, and race freedom |
| Fault recovery | Invalid input, partial failure, retry limits, post-timeout action, and safe-state recovery |
| Portability | Target-profile compliance, language rules, alignment, and undefined behavior |
| Clarity and validation | Reviewable implementation, stated assumptions, and focused tests |

## Requirement Ownership

Assign every prompt requirement to exactly one dimension before scoring. Deduct
only from that owning dimension when a defect has downstream effects elsewhere.
Deduct from multiple dimensions only for independent defects, and identify each
defect separately in the scoring notes.

- Timing owns blocking, busy-waiting, deadline detection, and wrap-safe time
  comparisons; bounded resource use owns memory and storage limits. Timing also
  owns whether a required primitive is lock-free, while concurrency safety owns
  whether its synchronization and memory ordering are correct.
- Concurrency safety owns overlapping asynchronous operations and shared-state
  synchronization; timing owns whether an operation waits for completion.
- Fault recovery owns the response after a timeout; timing owns whether the
  timeout is detected at the required deadline.
- Functional correctness owns successful results; fault recovery owns error
  classification, output clearing, retries, and recovery.

## Scoring Procedure

1. Blind model identity and confirm the task prompt, target profile, and rubric
   version match the compared runs.
2. Run the task's declared deterministic validation where available. Treat
   compiler, test, mutation, and sandbox results as evidence for the affected
   dimensions, not as an automatic total score.
3. Award points independently within each dimension. Partial credit is allowed;
   record the owning dimension and a concise, behavior-specific reason for
   every deduction.
4. Give zero points for a requirement contradicted by executable evidence.
   Do not infer missing code, repair an answer, or deduct for the same defect in
   multiple dimensions.
5. Record the profile and task-rubric revision in the score set, for example
   `firmware-v1 at <git-commit>`.

Safety-critical prompt violations receive zero in their owning dimension even
if public tests happen to pass. For example, overlapping peripheral operations
and non-atomic ISR-shared state belong to concurrency safety, while busy-waiting
belongs to timing behavior.

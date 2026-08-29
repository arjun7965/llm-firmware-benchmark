# Model-Family Calibration

Cross-model calibration checks whether a task is answerable, executable, and
consistently scored before its results are treated as benchmark evidence. It
does not replace fixture mutation testing or blinded rubric review.

## Protocol

For each task:

1. Run the trusted reference and require every controlled mutation to be
   rejected in the task's declared validation environment.
2. Select at least three distinct model families and generate three independent
   samples from each. Keep the task prompt and task metadata identical.
3. Treat every raw result file as immutable once written. The harness preserves
   successful results but replaces unsuccessful records when the same jobs run
   again, so use a new attempt-specific output directory for every retry and
   retain the original failure evidence.
4. Record model IDs, provider adapters, material provider options, prompt and
   provider-configuration hashes, run count, and tool versions. Disclose
   provider differences rather than treating different adapters as identical.
5. Extract each answer through the manifest-owned answer contract, then run it
   in the same validation-profile and environment revision used by every other
   sample in the comparison.
6. Keep raw provider records and generated answers under ignored paths. Publish
   answer text only through the reviewed public-result export process.
7. Blind model identities before assigning rubric scores. An executable pass
   is validation evidence, not an automatic score of 10.

Create the private scoring artifacts under ignored `results/` paths:

```bash
npm run calibration:blind -- \
  --input results/<pilot> \
  --task <task-id> \
  --output results/<pilot>/blind-scoring
```

The command extracts complete answers through the provider envelope parser,
rejects failed results and normalized literal model/provider identifiers in
answer text, randomly assigns `sample-NN` identifiers, and writes the answer
packet, a readable Markdown rendering, blank score sheet, and identity key
separately. Open `packet.md` for review instead of reading JSON-escaped answer
strings. It preserves line breaks and inner code fences inside inert Markdown
source blocks, and records the canonical `packet.json` and per-answer SHA-256
values. Score validation remains bound to `packet.json`. The reviewer packet
omits the rubric's `## Calibration` section so prior outcomes do not anchor the
new review while retaining the prompt, scoring criteria, and other rubric
sections. Manually review the packet for aliases or stylistic identity clues
that literal screening cannot detect. The packet records the sealed identity
key's SHA-256, committing the model/run mapping before review without
disclosing it. The key contains a random 256-bit nonce, preventing a reviewer
from brute-forcing the small permutation space from the public commitment.
Give a reviewer only
`packet.md`, `packet.json`, and `score-sheet.json`; tell them to open the
Markdown view and leave both packet files unchanged. Complete the sheet and
preserve its SHA-256 before anyone opens `identity-key.json`. After that
boundary, validate the key commitment, packet digest, answer digests, rubric
bounds, arithmetic, and model/run uniqueness while summarizing the scores:

```bash
npm run calibration:summarize -- \
  --directory results/<pilot>/blind-scoring
```

Packet inclusion attests only that provider generation succeeded and the
provider envelope contained an extractable answer. Before giving the packet to
a reviewer, separately verify the manifest-owned answer extraction and the
deterministic validation reports required by steps 1 and 5.

A task completes the executable part of this protocol when its trusted
reference passes, all controlled mutations are rejected, and every selected
sample has current prompt provenance plus a recorded extraction and validation
outcome. A model failure is useful calibration evidence and must not be silently
rerun away.

## Completed Pilots

### `static-memory-pool` — 2026-08-25

The pilot used the unchanged task prompt with SHA-256
`fc2d1a32d946e0221c345df2fdfd472a1498a448d3aaaa3bb75906e2e48e6141`.
It ran from harness commit `c1416a3` under Node.js 22.21.0. Each family
produced three independent samples:

```bash
npm run benchmark -- \
  --models gpt-5.6-luna,glm53,kimi-k3 \
  --tasks static-memory-pool \
  --runs 1,2,3 \
  --concurrency 3 \
  --output results/static-memory-pool-cross-family-20260825
```

| Model family | Provider path | Material options | Generation | Validation |
| --- | --- | --- | --- | --- |
| GPT-5.6 Luna | Codex CLI 0.149.1 | `effort=medium`, 600 s timeout | 3/3 | 3/3 |
| GLM-5.3 | OpenCode 1.18.23 | 900 s timeout | 3/3 | 3/3 |
| Kimi K3 | OpenCode 1.18.23 | `variant=max`, 600 s timeout | 3/3 | 3/3 |

The Codex adapter ran ephemerally in a read-only sandbox with tools and web
search disabled. The OpenCode adapter used its isolated benchmark agent with
all permissions denied; its provider-configuration SHA-256 was
`ab82a7cc2d121908666852f90bd880bebade7af665b8ab826965f4e493e889c6`.
The Codex adapter did not expose a provider-configuration fingerprint, so its
invocation controls and CLI version are the available provenance.

All nine answers used the required single `c` fence, remained below the
1,400-word limit, compiled without diagnostics, and passed the public tests.
Their answer SHA-256 values were all distinct. Validation used `c11-host`
revision 4, profile SHA-256
`366cfeebdef4d1b1144c4a4cc60184a02fed782c22e51e14209f55bc860ddcf3`,
and `debian-13-x86-64-c11-host` revision 1 with GCC 14.2.0 under Bubblewrap
0.11.0. The environment SHA-256 was
`3fa38109eeeef8b8bb87936ea357907e2b561c5b305b8dea54ea35ecb70401e7`.
The trusted reference passed, and the validator rejected all six compile-valid
controlled mutations.

Raw outputs, extracted answers, and per-sample validation reports remain under
`results/static-memory-pool-cross-family-20260825/` and are intentionally
Git-ignored.

### Preliminary blinded rubric review — 2026-08-27

The blinding workflow was exercised on all nine complete answers. Codex
GPT-5.6 Sol scored the randomized packet before the identity key was opened.
The packet SHA-256 was
`de1427141b7af701b1d79c685ff456fdac26ed1768dccab8c73126c1aafc0035`;
the completed score-sheet SHA-256, frozen before unblinding, was
`b903f99129397cfee44f2c5fbbf65745185628d9d1b9876b21655d6c5e9abca7`.

| Model family | Run scores | Mean | Population SD | Range |
| --- | --- | ---: | ---: | ---: |
| GPT-5.6 Luna | 10, 10, 10 | 10.000 | 0.000 | 0.0 |
| GLM-5.3 | 10, 10, 10 | 10.000 | 0.000 | 0.0 |
| Kimi K3 | 10, 9.5, 10 | 9.833 | 0.236 | 0.5 |

Across all nine samples, the mean was 9.944 with a population standard
deviation of 0.157. The sole deduction was half of the portability point: one
answer formed the exclusive storage end as `base + span`, which can wrap when
a valid pool occupies the top of a `uintptr_t` address space. Offset-first
bounds checking avoids that edge case.

This was an AI rubric review used to verify the blinding and score-validation
workflow. It is disclosed separately and does not satisfy the independent
blinded human-review gate for publication-grade benchmark scores.
This dry run predated the identity-key commitment now required by the workflow,
so its packet and score-sheet hashes do not independently prove that the
model/run mapping remained unchanged before unblinding.

### Independent blinded human review — 2026-08-28

An independent human reviewer scored a new packet prepared after the
identity-key commitment and rubric-anchoring protections were in place. The
review bundle contained only `packet.json` and the blank `score-sheet.json`;
the packet omitted the rubric's prior calibration outcomes. The completed
score sheet was frozen before unblinding and identified the scorer by the
pseudonym `reviewer-01`.

The packet SHA-256 was
`7263938819eec9bfbeb7c04cbfedd95b650cc50fa03e5a94654d3bc2bbea9bef`, and the
completed score-sheet SHA-256 was
`d724fe5a02eef9333c8c411abb07ea803af93ef8bdc088e7554be19873609af2`.
The calibration summarizer validated the identity-key commitment, packet and
answer digests, rubric bounds and arithmetic, and model/run uniqueness before
producing the following result:

| Model family | Run scores | Mean | Population SD | Range |
| --- | --- | ---: | ---: | ---: |
| GPT-5.6 Luna | 10, 10, 10 | 10.000 | 0.000 | 0.0 |
| GLM-5.3 | 10, 10, 10 | 10.000 | 0.000 | 0.0 |
| Kimi K3 | 10, 10, 10 | 10.000 | 0.000 | 0.0 |

Across all nine samples, the mean was 10.000 with a population standard
deviation and range of zero. The reviewed, sanitized machine-readable summary
is committed as
[`static-memory-pool-2026-08-28.json`](calibration/static-memory-pool-2026-08-28.json).
Raw outputs, answer text, the packet, the completed score sheet, and the
identity key remain private under ignored `results/` paths.

This completes the independent-review gate for this pilot, but one reviewer
and one task do not establish a broad model-family ranking. The uniform scores
also motivate selecting a task intended to provide a broader discrimination
surface for the next pilot.

## Selected Next Pilot

Next pilot: `fixed-point-filter-optimization`.

This active portable-C11 fixture moves calibration into the still-partial
resource-optimization capability. Its deterministic cost model makes four-MAC
ordering and the 28-cycle budget observable, while signed Q1.15 rounding,
saturation, and transactional commit failure exercise numerical and state
reasoning. The trusted reference passes, and all 33 compile-valid controlled
mutations are rejected. This is intended to improve discrimination without
introducing physical-hardware or service-runtime variability.

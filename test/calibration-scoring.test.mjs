import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import {
  mkdirSync,
  mkdtempSync,
  readFileSync,
  rmSync,
  symlinkSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import {
  buildBlindedScoringArtifacts,
  loadCalibrationSamples,
  parseCalibrationRubric,
  renderBlindedReviewPacket,
  resolvePrivateResultsPath,
  summarizeCompletedCalibrationScoring,
} from "../src/calibration-scoring.mjs";
import { loadTasks, promptSha256 } from "../src/harness.mjs";

const task = {
  category: "embedded",
  id: "example-task",
  prompt: "Return one fenced C implementation and an explanation.",
  scoringMode: "deterministic",
  suite: "firmware",
  targetProfile: "portable-c11",
  validationProfile: "c11-host",
};
const rubric = [
  "# Example Task",
  "",
  "- 4 points — **Functional correctness:** The implementation works.",
  "- 2 points — **Bounded resource use:** Work is bounded.",
  "- 1 point — **Timing behavior:** Timing is deterministic.",
  "- 1 point — **Concurrency safety:** State stays coherent.",
  "- 1 point — **Portability:** The code is portable.",
  "- 1 point — **Clarity and validation:** Tests are explained.",
  "",
].join("\n");

function temporaryDirectory(t) {
  const path = mkdtempSync(join(tmpdir(), "calibration-scoring-test-"));
  t.after(() => rmSync(path, { recursive: true, force: true }));
  return path;
}

function writeResult(root, {
  answer,
  modelId,
  modelName,
  run,
  ...overrides
}) {
  const directory = run === 1 ? root : join(root, `run-${run}`);
  mkdirSync(directory, { recursive: true });
  writeFileSync(join(directory, `${task.id}--${modelName}.json`), JSON.stringify({
    run,
    task: task.id,
    category: task.category,
    scoringMode: task.scoringMode,
    suite: task.suite,
    targetProfile: task.targetProfile,
    validationProfile: task.validationProfile,
    provider: "test-provider",
    modelName,
    modelId,
    promptSha256: promptSha256(task.prompt),
    exitCode: 0,
    signal: null,
    error: null,
    stdout: answer,
    ...overrides,
  }));
}

test("calibration samples are loaded without provider envelopes", (t) => {
  const root = temporaryDirectory(t);
  writeResult(root, {
    answer: "```c\nint first(void) { return 1; }\n```\nFirst explanation.",
    modelId: "provider/alpha",
    modelName: "model-alpha",
    run: 1,
  });
  writeResult(root, {
    answer: "```c\nint second(void) { return 2; }\n```\nSecond explanation.",
    modelId: "provider/beta",
    modelName: "model-beta",
    run: 2,
  });

  const samples = loadCalibrationSamples(root, task);
  assert.equal(samples.length, 2);
  assert.deepEqual(samples.map((sample) => sample.run), [1, 2]);
  assert.match(samples[0].answer, /First explanation/u);
});

test("calibration samples must match task evaluation metadata", (t) => {
  const root = temporaryDirectory(t);
  writeResult(root, {
    answer: "answer",
    modelId: "provider/alpha",
    modelName: "model-alpha",
    run: 1,
    validationProfile: "other-profile",
  });

  assert.throws(
    () => loadCalibrationSamples(root, task),
    /validationProfile does not match the task/u,
  );
});

test("blinded artifacts separate answers from the identity key", () => {
  const samples = [
    {
      answer: "answer alpha",
      answerSha256: createHash("sha256")
        .update("answer alpha")
        .digest("hex"),
      modelId: "provider/alpha",
      modelName: "model-alpha",
      provider: "provider-a",
      run: 1,
      source: "example-task--model-alpha.json",
    },
    {
      answer: "answer beta",
      answerSha256: createHash("sha256")
        .update("answer beta")
        .digest("hex"),
      modelId: "provider/beta",
      modelName: "model-beta",
      provider: "provider-b",
      run: 1,
      source: "example-task--model-beta.json",
    },
  ];
  const artifacts = buildBlindedScoringArtifacts({
    createCommitmentNonce: () => "c".repeat(64),
    rubric,
    samples,
    task,
    chooseIndex: () => 0,
  });

  assert.equal(artifacts.packet.samples[0].answer, "answer beta");
  assert.equal(artifacts.key.samples[0].modelName, "model-beta");
  assert.doesNotMatch(artifacts.packetText, /model-alpha|model-beta|provider-a/u);
  assert.doesNotMatch(
    artifacts.packetMarkdownText,
    /model-alpha|model-beta|provider-a/u,
  );
  assert.match(artifacts.keyText, /model-alpha|model-beta/u);
  assert.equal(artifacts.key.commitmentNonce, "c".repeat(64));
  assert.doesNotMatch(artifacts.packetText, /c{64}/u);
  assert.match(
    artifacts.packet.generationEvidence,
    /does not attest either outcome/u,
  );
  assert.equal(
    artifacts.packet.identityKeySha256,
    createHash("sha256").update(artifacts.keyText).digest("hex"),
  );
  assert.deepEqual(
    Object.keys(artifacts.scoreSheet.samples[0].scores),
    artifacts.scoreSheet.criteria.map((criterion) => criterion.id),
  );
});

test("blinded packets include a readable digest-bound Markdown view", () => {
  const answer = [
    "```c",
    "int example(void)",
    "{",
    "    return 1;",
    "}",
    "```",
    "",
    "The implementation is deterministic.",
  ].join("\n");
  const artifacts = buildBlindedScoringArtifacts({
    createCommitmentNonce: () => "c".repeat(64),
    rubric,
    samples: [{
      answer,
      answerSha256: createHash("sha256").update(answer).digest("hex"),
      modelId: "provider/alpha",
      modelName: "model-alpha",
      provider: "provider-a",
      run: 1,
      source: "example-task--model-alpha.json",
    }],
    task,
    chooseIndex: () => 0,
  });
  const packetDigest = createHash("sha256")
    .update(artifacts.packetText)
    .digest("hex");

  assert.match(
    artifacts.packetMarkdownText,
    /^# Blinded Calibration Review Packet$/mu,
  );
  assert.ok(artifacts.packetMarkdownText.includes(packetDigest));
  assert.ok(
    artifacts.packetMarkdownText.includes(artifacts.packet.identityKeySha256),
  );
  assert.ok(artifacts.packetMarkdownText.includes(answer));
  assert.match(
    artifacts.packetMarkdownText,
    /````markdown\n```c\nint example\(void\)/u,
  );
  assert.doesNotMatch(artifacts.packetMarkdownText, /\\nint example/u);
  assert.doesNotMatch(artifacts.packetMarkdownText, /model-alpha|provider-a/u);
  assert.equal(
    renderBlindedReviewPacket(artifacts.packet, artifacts.packetText),
    artifacts.packetMarkdownText,
  );

  const tamperedPacket = structuredClone(artifacts.packet);
  tamperedPacket.samples[0].answer += "\ntampered";
  const tamperedPacketText = `${JSON.stringify(tamperedPacket, null, 2)}\n`;
  assert.throws(
    () => renderBlindedReviewPacket(tamperedPacket, tamperedPacketText),
    /answer digest does not match/u,
  );
});

test("blinded packets omit prior calibration outcomes from the rubric", () => {
  const artifacts = buildBlindedScoringArtifacts({
    createCommitmentNonce: () => "c".repeat(64),
    rubric: [
      rubric.trimEnd(),
      "",
      "## Calibration",
      "",
      "A prior reviewer gave one answer a lower score.",
      "",
      "### Details",
      "",
      "This outcome must not anchor the next reviewer.",
      "",
      "## Source Provenance",
      "",
      "The task uses a repository-authored interface.",
      "",
    ].join("\n"),
    samples: [{
      answer: "answer alpha",
      answerSha256: createHash("sha256")
        .update("answer alpha")
        .digest("hex"),
      modelId: "provider/alpha",
      modelName: "model-alpha",
      provider: "provider-a",
      run: 1,
      source: "example-task--model-alpha.json",
    }],
    task,
    chooseIndex: () => 0,
  });

  assert.doesNotMatch(artifacts.packet.task.rubric, /prior reviewer|anchor/u);
  assert.doesNotMatch(artifacts.packet.task.rubric, /^## Calibration$/mu);
  assert.match(artifacts.packet.task.rubric, /^## Source Provenance$/mu);
  assert.equal(artifacts.packet.task.rubricCriteria.length, 6);
});

test("rubric parsing requires an exact ten-point allocation", () => {
  assert.equal(parseCalibrationRubric(rubric).length, 6);
  assert.throws(
    () => parseCalibrationRubric(rubric.replace("4 points", "3 points")),
    /total 9, expected 10/u,
  );
});

test("private calibration paths reject escape and symlink traversal", (t) => {
  const root = temporaryDirectory(t);
  const results = join(root, "results");
  const pilot = join(results, "pilot");
  const outside = join(root, "outside");
  mkdirSync(pilot, { recursive: true });
  mkdirSync(outside);
  symlinkSync(outside, join(results, "linked"));

  assert.equal(
    resolvePrivateResultsPath(pilot, { results }),
    pilot,
  );
  assert.equal(
    resolvePrivateResultsPath(join(pilot, "new"), {
      mustExist: false,
      results,
    }),
    join(pilot, "new"),
  );
  assert.throws(
    () => resolvePrivateResultsPath(outside, { results }),
    /inside the ignored results directory/u,
  );
  assert.throws(
    () => resolvePrivateResultsPath(join(results, "linked"), { results }),
    /cannot contain symlinks/u,
  );
});

test("completed blind scores are verified before model summaries", () => {
  const samples = [
    {
      answer: "answer alpha",
      answerSha256: createHash("sha256").update("answer alpha").digest("hex"),
      modelId: "provider/alpha",
      modelName: "model-alpha",
      provider: "provider-a",
      run: 1,
      source: "example-task--model-alpha.json",
    },
    {
      answer: "answer beta",
      answerSha256: createHash("sha256").update("answer beta").digest("hex"),
      modelId: "provider/beta",
      modelName: "model-beta",
      provider: "provider-b",
      run: 1,
      source: "example-task--model-beta.json",
    },
  ];
  const artifacts = buildBlindedScoringArtifacts({
    createCommitmentNonce: () => "d".repeat(64),
    rubric,
    samples,
    task,
    chooseIndex: (upper) => upper - 1,
  });
  const scoreSheet = structuredClone(artifacts.scoreSheet);
  scoreSheet.status = "complete";
  scoreSheet.scorer = {
    completedAt: "2026-08-27T20:00:00Z",
    identity: "reviewer",
    type: "human",
  };
  for (const sample of scoreSheet.samples) {
    sample.scores = Object.fromEntries(
      scoreSheet.criteria.map((criterion) => [criterion.id, criterion.maximum]),
    );
    sample.total = 10;
    sample.rationale = "Meets every criterion.";
  }
  const scoreSheetText = `${JSON.stringify(scoreSheet, null, 2)}\n`;
  const summary = summarizeCompletedCalibrationScoring({
    identityKey: artifacts.key,
    identityKeyText: artifacts.keyText,
    packet: artifacts.packet,
    packetText: artifacts.packetText,
    scoreSheet,
    scoreSheetText,
  });

  assert.deepEqual(summary.models.map(({ model, mean }) => ({ model, mean })), [
    { model: "model-alpha", mean: 10 },
    { model: "model-beta", mean: 10 },
  ]);
  assert.equal(summary.overall.sampleCount, 2);
  assert.equal(summary.overall.sd, 0);

  const invalidScoreSheet = structuredClone(scoreSheet);
  invalidScoreSheet.samples[0].scores["functional-correctness"] = 5;
  const invalidScoreSheetText =
    `${JSON.stringify(invalidScoreSheet, null, 2)}\n`;
  assert.throws(
    () => summarizeCompletedCalibrationScoring({
      identityKey: artifacts.key,
      identityKeyText: artifacts.keyText,
      packet: artifacts.packet,
      packetText: artifacts.packetText,
      scoreSheet: invalidScoreSheet,
      scoreSheetText: invalidScoreSheetText,
    }),
    /outside its range/u,
  );

  const remappedKey = structuredClone(artifacts.key);
  remappedKey.samples[0].modelName = "replacement-model";
  const remappedKeyText = `${JSON.stringify(remappedKey, null, 2)}\n`;
  assert.throws(
    () => summarizeCompletedCalibrationScoring({
      identityKey: remappedKey,
      identityKeyText: remappedKeyText,
      packet: artifacts.packet,
      packetText: artifacts.packetText,
      scoreSheet,
      scoreSheetText,
    }),
    /identity key digest does not match/u,
  );
});

test("sample loading rejects failed, duplicate, and identity-revealing results", (t) => {
  const failedRoot = temporaryDirectory(t);
  writeResult(failedRoot, {
    answer: "answer",
    modelId: "provider/failed",
    modelName: "model-failed",
    run: 1,
    exitCode: 1,
  });
  assert.throws(
    () => loadCalibrationSamples(failedRoot, task),
    /was not successful/u,
  );

  const duplicateRoot = temporaryDirectory(t);
  writeResult(duplicateRoot, {
    answer: "answer one",
    modelId: "provider/alpha",
    modelName: "model-alpha",
    run: 1,
  });
  mkdirSync(join(duplicateRoot, "attempt-2"));
  writeFileSync(
    join(duplicateRoot, "attempt-2", `${task.id}--model-alpha.json`),
    JSON.stringify({
      run: 1,
      task: task.id,
      category: task.category,
      scoringMode: task.scoringMode,
      suite: task.suite,
      targetProfile: task.targetProfile,
      validationProfile: task.validationProfile,
      provider: "test-provider",
      modelName: "model-alpha",
      modelId: "provider/alpha",
      promptSha256: promptSha256(task.prompt),
      exitCode: 0,
      signal: null,
      error: null,
      stdout: "answer two",
    }),
  );
  assert.throws(
    () => loadCalibrationSamples(duplicateRoot, task),
    /duplicate calibration result/u,
  );

  const leakRoot = temporaryDirectory(t);
  writeResult(leakRoot, {
    answer: "Generated by MODEL ALPHA.",
    modelId: "provider/alpha",
    modelName: "model-alpha",
    run: 1,
  });
  assert.throws(
    () => loadCalibrationSamples(leakRoot, task),
    /exposes its model identity/u,
  );

  const providerLeakRoot = temporaryDirectory(t);
  writeResult(providerLeakRoot, {
    answer: "Generated by CODEX.",
    modelId: "unlisted-model",
    modelName: "unlisted-model",
    provider: "codex",
    run: 1,
  });
  assert.throws(
    () => loadCalibrationSamples(providerLeakRoot, task),
    /exposes its model identity/u,
  );
});

test("published static-memory-pool human scores match the reviewed summary", () => {
  const resultPath = new URL(
    "../docs/calibration/static-memory-pool-2026-08-28.json",
    import.meta.url,
  );
  const summary = JSON.parse(readFileSync(resultPath, "utf8"));

  assert.equal(summary.schemaVersion, "1.0");
  assert.equal(summary.taskId, "static-memory-pool");
  assert.equal(
    summary.packetSha256,
    "7263938819eec9bfbeb7c04cbfedd95b650cc50fa03e5a94654d3bc2bbea9bef",
  );
  assert.equal(
    summary.scoreSheetSha256,
    "d724fe5a02eef9333c8c411abb07ea803af93ef8bdc088e7554be19873609af2",
  );
  assert.deepEqual(summary.scorer, {
    completedAt: "2026-08-28T05:39:03Z",
    identity: "reviewer-01",
    type: "human",
  });
  assert.deepEqual(
    summary.models.map(({ model }) => model).sort(),
    ["glm53", "gpt-5.6-luna", "kimi-k3"],
  );

  const scores = [];
  for (const model of summary.models) {
    assert.deepEqual(model.runs.map(({ run }) => run), [1, 2, 3]);
    assert.deepEqual(model.runs.map(({ score }) => score), [10, 10, 10]);
    assert.deepEqual(
      { mean: model.mean, range: model.range, sd: model.sd },
      { mean: 10, range: 0, sd: 0 },
    );
    scores.push(...model.runs.map(({ score }) => score));
  }
  const mean = scores.reduce((sum, score) => sum + score, 0) / scores.length;
  const variance = scores.reduce(
    (sum, score) => sum + (score - mean) ** 2,
    0,
  ) / scores.length;
  assert.deepEqual(summary.overall, {
    sampleCount: scores.length,
    mean,
    range: Math.max(...scores) - Math.min(...scores),
    sd: Math.sqrt(variance),
  });

  const calibration = readFileSync(
    new URL("../docs/model-family-calibration.md", import.meta.url),
    "utf8",
  );
  const rubric = readFileSync(
    new URL("../docs/benchmarks/static-memory-pool.md", import.meta.url),
    "utf8",
  );
  const todo = readFileSync(new URL("../TODO.md", import.meta.url), "utf8");
  assert.match(
    calibration,
    /\[`static-memory-pool-2026-08-28\.json`\]\(calibration\/static-memory-pool-2026-08-28\.json\)/u,
  );
  assert.match(
    rubric,
    /\[`static-memory-pool-2026-08-28\.json`\]\(\.\.\/calibration\/static-memory-pool-2026-08-28\.json\)/u,
  );
  assert.ok(calibration.includes(summary.packetSha256));
  assert.ok(calibration.includes(summary.scoreSheetSha256));
  const displayNames = new Map([
    ["glm53", "GLM-5.3"],
    ["gpt-5.6-luna", "GPT-5.6 Luna"],
    ["kimi-k3", "Kimi K3"],
  ]);
  for (const model of summary.models) {
    const scoresText = model.runs.map(({ score }) => score).join(", ");
    const expectedRow = [
      displayNames.get(model.model),
      scoresText,
      model.mean.toFixed(3),
      model.sd.toFixed(3),
      model.range.toFixed(1),
    ].join(" | ");
    assert.ok(
      calibration.includes(`| ${expectedRow} |`),
      `missing published score row for ${model.model}`,
    );
  }
  assert.match(
    calibration,
    /Across all nine samples, the mean was 10\.000 with a population standard\s+deviation and range of zero\./u,
  );
  assert.match(
    rubric,
    /reviewer scored all nine answers at 10;\s+the aggregate mean was 10\.000 with zero population standard deviation and\s+range\./u,
  );
  assert.match(
    todo,
    /- \[x\] Obtain independent blinded human scores for the `static-memory-pool`/u,
  );
});

test("published fixed-point-filter human scores match the reviewed summary", () => {
  const resultPath = new URL(
    "../docs/calibration/fixed-point-filter-optimization-2026-08-28.json",
    import.meta.url,
  );
  const summary = JSON.parse(readFileSync(resultPath, "utf8"));

  assert.equal(summary.schemaVersion, "1.0");
  assert.equal(summary.taskId, "fixed-point-filter-optimization");
  assert.equal(
    summary.packetSha256,
    "790c322abcfa185f25614106db72e565657dad89f763cc03a1a63ab9de6c2c58",
  );
  assert.equal(
    summary.scoreSheetSha256,
    "b30576dd226ab56a8f7b70d07585b53ee8431c54afa786a14ac924c831be5bc4",
  );
  assert.deepEqual(summary.scorer, {
    completedAt: "2026-08-28T22:40:29-07:00",
    identity: "reviewer-me",
    type: "human",
  });
  assert.deepEqual(
    summary.models.map(({ model }) => model).sort(),
    ["glm53", "gpt-5.6-luna", "kimi-k3"],
  );

  const scores = [];
  for (const model of summary.models) {
    assert.deepEqual(model.runs.map(({ run }) => run), [1, 2, 3]);
    assert.deepEqual(model.runs.map(({ score }) => score), [10, 10, 10]);
    assert.deepEqual(
      { mean: model.mean, range: model.range, sd: model.sd },
      { mean: 10, range: 0, sd: 0 },
    );
    scores.push(...model.runs.map(({ score }) => score));
  }
  const mean = scores.reduce((sum, score) => sum + score, 0) / scores.length;
  const variance = scores.reduce(
    (sum, score) => sum + (score - mean) ** 2,
    0,
  ) / scores.length;
  assert.deepEqual(summary.overall, {
    sampleCount: scores.length,
    mean,
    range: Math.max(...scores) - Math.min(...scores),
    sd: Math.sqrt(variance),
  });

  const calibration = readFileSync(
    new URL("../docs/model-family-calibration.md", import.meta.url),
    "utf8",
  );
  const rubric = readFileSync(
    new URL(
      "../docs/benchmarks/fixed-point-filter-optimization.md",
      import.meta.url,
    ),
    "utf8",
  );
  const todo = readFileSync(new URL("../TODO.md", import.meta.url), "utf8");
  assert.match(
    calibration,
    /\[`fixed-point-filter-optimization-2026-08-28\.json`\]\(calibration\/fixed-point-filter-optimization-2026-08-28\.json\)/u,
  );
  assert.match(
    rubric,
    /\[`fixed-point-filter-optimization-2026-08-28\.json`\]\(\.\.\/calibration\/fixed-point-filter-optimization-2026-08-28\.json\)/u,
  );
  assert.ok(calibration.includes(summary.packetSha256));
  assert.ok(calibration.includes(summary.scoreSheetSha256));
  const displayNames = new Map([
    ["glm53", "GLM-5.3"],
    ["gpt-5.6-luna", "GPT-5.6 Luna"],
    ["kimi-k3", "Kimi K3"],
  ]);
  for (const model of summary.models) {
    const scoresText = model.runs.map(({ score }) => score).join(", ");
    const expectedRow = [
      displayNames.get(model.model),
      scoresText,
      model.mean.toFixed(3),
      model.sd.toFixed(3),
      model.range.toFixed(1),
    ].join(" | ");
    assert.ok(
      calibration.includes(`| ${expectedRow} |`),
      `missing published score row for ${model.model}`,
    );
  }
  assert.match(
    calibration,
    /Across all nine samples, the mean was 10\.000 with a population standard\s+deviation and range of zero\./u,
  );
  assert.match(
    rubric,
    /reviewer scored all nine answers at 10; the\s+aggregate mean was 10\.000 with zero population standard deviation and range\./u,
  );
  assert.match(
    todo,
    /- \[x\] Run the `fixed-point-filter-optimization` cross-model pilot/u,
  );
});

test("the selected next calibration pilot is active and deterministic", () => {
  const calibration = readFileSync(
    new URL("../docs/model-family-calibration.md", import.meta.url),
    "utf8",
  );
  const matches = [...calibration.matchAll(/^Next pilot: `([^`]+)`\.$/gmu)];
  assert.equal(matches.length, 1, "calibration guide must name one next pilot");

  const taskId = matches[0][1];
  const selectedTask = loadTasks(
    new URL("../tasks.json", import.meta.url),
  ).find((candidate) => candidate.id === taskId);
  assert.ok(selectedTask, `unknown next calibration task: ${taskId}`);
  assert.equal(selectedTask.scoringMode, "deterministic");
  assert.equal(selectedTask.suite, "firmware");

  const fixtureRoot = new URL(`../fixtures/${taskId}/`, import.meta.url);
  const manifest = JSON.parse(readFileSync(
    new URL("manifest.json", fixtureRoot),
    "utf8",
  ));
  const mutations = JSON.parse(readFileSync(
    new URL("mutations.json", fixtureRoot),
    "utf8",
  ));
  assert.equal(manifest.taskId, taskId);
  assert.equal(manifest.status, "active");
  assert.equal(manifest.validationProfile, selectedTask.validationProfile);
  const nextPilotSection = calibration.slice(matches[0].index);
  assert.match(
    nextPilotSection,
    new RegExp(
      "revised prompt SHA-256 is\\s+`" +
        promptSha256(selectedTask.prompt) +
        "`",
      "u",
    ),
  );
  const mutationCount = nextPilotSection.match(
    /all ([0-9]+) compile-valid controlled\s+mutations are rejected/u,
  );
  assert.ok(mutationCount, "next-pilot rationale must record mutation coverage");
  assert.equal(Number(mutationCount[1]), mutations.mutations.length);

  const todo = readFileSync(new URL("../TODO.md", import.meta.url), "utf8");
  assert.match(
    todo,
    new RegExp(
      "- \\[x\\] Select `" + taskId + "` as the next deterministic task",
      "u",
    ),
  );
  assert.match(
    todo,
    new RegExp(
      "- \\[ \\] Run the `" + taskId + "` cross-model pilot",
      "u",
    ),
  );
});

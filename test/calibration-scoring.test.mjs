import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import {
  mkdirSync,
  mkdtempSync,
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
  resolvePrivateResultsPath,
  summarizeCompletedCalibrationScoring,
} from "../src/calibration-scoring.mjs";
import { promptSha256 } from "../src/harness.mjs";

const task = {
  id: "example-task",
  prompt: "Return one fenced C implementation and an explanation.",
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

test("blinded artifacts separate answers from the identity key", () => {
  const samples = [
    {
      answer: "answer alpha",
      answerSha256: "a".repeat(64),
      modelId: "provider/alpha",
      modelName: "model-alpha",
      provider: "provider-a",
      run: 1,
      source: "example-task--model-alpha.json",
    },
    {
      answer: "answer beta",
      answerSha256: "b".repeat(64),
      modelId: "provider/beta",
      modelName: "model-beta",
      provider: "provider-b",
      run: 1,
      source: "example-task--model-beta.json",
    },
  ];
  const artifacts = buildBlindedScoringArtifacts({
    rubric,
    samples,
    task,
    chooseIndex: () => 0,
  });

  assert.equal(artifacts.packet.samples[0].answer, "answer beta");
  assert.equal(artifacts.key.samples[0].modelName, "model-beta");
  assert.doesNotMatch(artifacts.packetText, /model-alpha|model-beta|provider-a/u);
  assert.match(artifacts.keyText, /model-alpha|model-beta/u);
  assert.equal(
    artifacts.key.packetSha256,
    createHash("sha256").update(artifacts.packetText).digest("hex"),
  );
  assert.deepEqual(
    Object.keys(artifacts.scoreSheet.samples[0].scores),
    artifacts.scoreSheet.criteria.map((criterion) => criterion.id),
  );
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

  scoreSheet.samples[0].scores["functional-correctness"] = 5;
  assert.throws(
    () => summarizeCompletedCalibrationScoring({
      identityKey: artifacts.key,
      packet: artifacts.packet,
      packetText: artifacts.packetText,
      scoreSheet,
      scoreSheetText,
    }),
    /outside its range/u,
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
    answer: "Generated by model-alpha.",
    modelId: "provider/alpha",
    modelName: "model-alpha",
    run: 1,
  });
  assert.throws(
    () => loadCalibrationSamples(leakRoot, task),
    /exposes its model identity/u,
  );
});

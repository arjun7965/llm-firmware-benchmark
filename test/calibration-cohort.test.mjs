import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import {
  buildBlindedScoringArtifacts,
  loadCalibrationCohort,
  summarizeCompletedCalibrationScoring,
  validateCalibrationPlan,
} from "../src/calibration-scoring.mjs";
import { loadTasks, promptSha256 } from "../src/harness.mjs";
import { describeGenerationBudget } from "../src/generation-budget.mjs";

const task = {
  id: "example-task", prompt: "Implement the supplied API.", category: "test",
  suite: "auxiliary", scoringMode: "deterministic", validationProfile: "c11-host",
};
const rubric = "# Example\n\n- 10 points — **Correctness:** Implements the API.\n";
const json = (value) => `${JSON.stringify(value, null, 2)}\n`;
function setup(t) {
  const root = mkdtempSync(join(tmpdir(), "calibration-cohort-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const plan = {
    schemaVersion: "1.0", taskId: task.id, promptSha256: promptSha256(task.prompt),
    models: ["alpha", "beta", "gamma"].map((name) => ({
      modelName: name, modelId: `provider/${name}`, provider: "opencode",
      family: name, modelOptions: { timeoutMs: 600000, variant: "high" },
      providerConfigSha256: null, runs: [1, 2, 3],
    })),
  };
  function write(modelName, run, overrides = {}, directory = root) {
    const model = plan.models.find((entry) => entry.modelName === modelName);
    const path = join(directory, `run-${run}`, `${task.id}--${modelName}.json`);
    mkdirSync(join(directory, `run-${run}`), { recursive: true });
    writeFileSync(path, json({
      ...model, run, task: task.id, promptSha256: plan.promptSha256,
      category: task.category, suite: task.suite, scoringMode: task.scoringMode,
      targetProfile: null, validationProfile: task.validationProfile,
      exitCode: 0, signal: null, error: null, stdout: "An implementation.",
      stderr: "", ...overrides,
    }));
    return path;
  }
  return { root, plan, write };
}
function score(artifacts) {
  const scoreSheet = structuredClone(artifacts.scoreSheet);
  scoreSheet.status = "complete";
  scoreSheet.scorer = {
    completedAt: "2026-09-06T00:00:00Z", identity: "reviewer", type: "human",
  };
  for (const sample of scoreSheet.samples) {
    sample.scores = { correctness: 7 };
    sample.total = 7;
    sample.rationale = "Partial implementation; lifecycle errors.";
  }
  return { scoreSheet, scoreSheetText: json(scoreSheet) };
}
function summarize(artifacts, changes = {}) {
  return summarizeCompletedCalibrationScoring({
    identityKey: artifacts.key, identityKeyText: artifacts.keyText,
    packet: artifacts.packet, packetText: artifacts.packetText,
    ...score(artifacts), ...changes,
  });
}

test("cohort summary retains every scheduled attempt without inventing scores", (t) => {
  const { root, plan, write } = setup(t);
  write("alpha", 1);
  write("alpha", 2, { exitCode: null, signal: "SIGTERM", error: "timed out after 600000 ms" });
  write("alpha", 3, { exitCode: 1, error: "no text", stdout: JSON.stringify({
    type: "step_finish", part: { reason: "length" },
  }) });
  for (const run of [1, 2, 3]) write("beta", run, {
    exitCode: 1, stderr: "database is locked", stdout: "",
  });
  write("gamma", 1, { stdout: "" });
  write("gamma", 2, { stdout: "```c\ninvalid C is still an answer\n```" });
  const loaded = loadCalibrationCohort(root, task, plan);
  assert.equal(loaded.samples.length, 2);
  const artifacts = buildBlindedScoringArtifacts({ ...loaded, task, rubric });
  assert.equal(artifacts.packet.schemaVersion, "1.1");
  assert.doesNotMatch(artifacts.packetText, /alpha|beta|gamma|database|variant/u);
  assert.equal(artifacts.scoreSheet.samples.length, 2);
  const summary = summarize(artifacts);
  assert.deepEqual(summary.generation, {
    scheduled: 9, recorded: 8, answers: 2, answerRate: 2 / 9,
    outcomes: { answer: 2, timeout: 1, "generation-limit": 1,
      "provider-error": 3, "no-answer": 1, missing: 1 },
  });
  assert.equal(summary.cohortRecorded, false);
  assert.equal(summary.plannedFamilyCount, 3);
  assert.equal(summary.overall.mean, 7);
  assert.equal(summary.overall.sampleCount, 2);
  const beta = summary.models.find((model) => model.model === "beta");
  assert.deepEqual(beta.runs, []);
  assert.equal(beta.mean, null);
  assert.equal(beta.sd, null);
  assert.equal(beta.generation.answerRate, 0);
  assert.equal(beta.generationRuns[0].generationBudget.timeoutMs, 600000);
  assert.equal(beta.generationRuns[0].generationBudget.providerTokenLimits, null);

  const key = structuredClone(artifacts.key);
  key.cohort.attempts[1].outcome = "missing";
  assert.throws(() => summarize(artifacts, {
    identityKey: key, identityKeyText: json(key),
  }), /identity key digest/u);
  assert.throws(() => summarize(artifacts, {
    scoreSheet: artifacts.scoreSheet, scoreSheetText: artifacts.scoreSheetText,
  }), /not complete/u);
});

test("an entirely failed cohort can be reviewed without fabricated answer scores", (t) => {
  const { root, plan, write } = setup(t);
  for (const model of plan.models) for (const run of model.runs) {
    write(model.modelName, run, { exitCode: 1, stdout: "" });
  }
  const artifacts = buildBlindedScoringArtifacts({
    ...loadCalibrationCohort(root, task, plan), task, rubric,
  });
  assert.deepEqual(artifacts.packet.samples, []);
  const summary = summarize(artifacts);
  assert.equal(summary.cohortRecorded, true);
  assert.equal(summary.generation.recorded, 9);
  assert.equal(summary.overall.mean, null);
  assert.equal(summary.overall.range, null);
  assert.equal(summary.overall.sampleCount, 0);
  assert.equal(summary.models.length, 3);
});

test("missing results remain visible, while retries and configuration drift are rejected", (t) => {
  const { root, plan, write } = setup(t);
  assert.equal(loadCalibrationCohort(root, task, plan).cohort.attempts.length, 9);
  write("alpha", 1, { exitCode: 1, stdout: "" });
  write("alpha", 1, {}, join(root, "retry"));
  assert.throws(() => loadCalibrationCohort(root, task, plan), /duplicate cohort run/u);
  rmSync(join(root, "retry"), { recursive: true });
  write("alpha", 1, { modelOptions: { timeoutMs: 600000, variant: "low" } });
  assert.throws(() => loadCalibrationCohort(root, task, plan), /modelOptions does not match/u);
  write("alpha", 1, { exitCode: 1, promptSha256: "0".repeat(64) });
  assert.throws(() => loadCalibrationCohort(root, task, plan), /prompt does not match/u);
  write("alpha", 1, { providerConfigSha256: "a".repeat(64) });
  assert.throws(() => loadCalibrationCohort(root, task, plan), /providerConfigSha256/u);
  write("alpha", 1);
  write("alpha", 4);
  assert.throws(() => loadCalibrationCohort(root, task, plan), /outside the declared cohort/u);
});

test("cohort loading rejects malformed records and identity leaks instead of hiding them", (t) => {
  const { root, plan, write } = setup(t);
  write("alpha", 1, { stdout: "I am ALPHA." });
  assert.throws(() => loadCalibrationCohort(root, task, plan), /exposes its model identity/u);
  const path = write("alpha", 1);
  const raw = JSON.parse(readFileSync(path));
  delete raw.exitCode;
  writeFileSync(path, json(raw));
  assert.throws(() => loadCalibrationCohort(root, task, plan), /malformed execution/u);
  write("alpha", 1, { generationBudget: { privateData: "must not propagate" } });
  assert.throws(() => loadCalibrationCohort(root, task, plan), /generation budget does not match/u);
});

test("cohort attempts and counts are bound to the blinded packet", (t) => {
  const { root, plan, write } = setup(t);
  write("alpha", 1);
  const loaded = loadCalibrationCohort(root, task, plan);
  const changed = structuredClone(loaded);
  changed.cohort.attempts[0].outcome = "provider-error";
  assert.throws(() => buildBlindedScoringArtifacts({ ...changed, task, rubric }),
    /cannot have a rubric score/u);
  const artifacts = buildBlindedScoringArtifacts({ ...loaded, task, rubric });
  const packet = structuredClone(artifacts.packet);
  packet.generation.recorded = 9;
  const packetText = json(packet);
  const { scoreSheet } = score(artifacts);
  scoreSheet.packetSha256 = createHash("sha256").update(packetText).digest("hex");
  assert.throws(() => summarize(artifacts, {
    packet, packetText, scoreSheet, scoreSheetText: json(scoreSheet),
  }), /generation counts do not match/u);
});

test("generation budgets distinguish configured limits from unreported provider limits", () => {
  assert.deepEqual(describeGenerationBudget("opencode", { variant: "max" }), {
    timeoutMs: 600000, reasoning: { control: "variant", value: "max" },
    configuredTokenLimits: null, providerTokenLimits: null,
  });
  assert.equal(describeGenerationBudget("opencode", {}, false).timeoutMs, null);
  assert.equal(describeGenerationBudget("codex", {}, false).reasoning, null);
  const budget = describeGenerationBudget("openai-compatible", { request: {
    max_completion_tokens: 4096, reasoning_effort: "low", unrelated_option: "excluded",
  } });
  assert.deepEqual(budget.configuredTokenLimits, { max_completion_tokens: 4096 });
  assert.equal(budget.reasoning.value, "low");
  assert.doesNotMatch(JSON.stringify(budget), /excluded|unrelated_option/u);
});

test("calibration CLI prepares and summarizes a mixed cohort end to end", (t) => {
  const { root, plan, write } = setup(t);
  const input = join(root, "results", "pilot");
  mkdirSync(input, { recursive: true });
  write("alpha", 1, {}, input);
  for (const model of plan.models) for (const run of model.runs) {
    if (model.modelName !== "alpha" || run !== 1) {
      write(model.modelName, run, { exitCode: 1, stdout: "" }, input);
    }
  }
  const tasksPath = join(root, "tasks.json");
  const rubricPath = join(root, "rubric.md");
  const planPath = join(input, "cohort.json");
  const output = join(input, "blind-scoring");
  writeFileSync(tasksPath, json([task]));
  writeFileSync(rubricPath, rubric);
  writeFileSync(planPath, json(plan));
  const script = (name) => fileURLToPath(new URL(`../scripts/${name}.mjs`, import.meta.url));
  const prepared = spawnSync(process.execPath, [
    script("prepare-calibration-scoring"), "--input", input, "--output", output,
    "--cohort", planPath, "--tasks", tasksPath, "--rubric", rubricPath,
    "--task", task.id,
  ], { cwd: root, encoding: "utf8" });
  assert.equal(prepared.status, 0, prepared.stderr);
  const sheet = JSON.parse(readFileSync(join(output, "score-sheet.json")));
  assert.equal(sheet.samples.length, 1);
  const completed = score({ scoreSheet: sheet });
  writeFileSync(join(output, "score-sheet.json"), completed.scoreSheetText);
  const summary = spawnSync(process.execPath, [
    script("summarize-calibration-scoring"), "--directory", output,
  ], { cwd: root, encoding: "utf8" });
  assert.equal(summary.status, 0, summary.stderr);
  const parsed = JSON.parse(summary.stdout);
  assert.equal(parsed.generation.scheduled, 9);
  assert.equal(parsed.generation.recorded, 9);
  assert.equal(parsed.generation.answers, 1);
  assert.equal(parsed.overall.mean, 7);
  assert.equal(parsed.models.filter((model) => model.mean === null).length, 2);
});

test("the example cohort pins the supervisor's corrected environment prompt", () => {
  const tasks = loadTasks(new URL("../tasks.json", import.meta.url));
  const supervisor = tasks.find((entry) => entry.id === "supervised-process-service");
  const example = JSON.parse(readFileSync(
    new URL("../calibration-cohort.example.json", import.meta.url), "utf8",
  ));
  validateCalibrationPlan(example, supervisor);
  assert.match(supervisor.prompt, /debian-13-x86-64-c11-host revision 1/u);
  assert.match(supervisor.prompt, /GCC 14\.2\.0/u);
  assert.doesNotMatch(supervisor.prompt, /Ubuntu/u);
  const rubricText = readFileSync(
    new URL("../docs/benchmarks/supervised-process-service.md", import.meta.url), "utf8",
  );
  assert.match(rubricText, /`debian-13-x86-64-c11-host` revision 1/u);
  assert.match(rubricText, /GCC 14\.2\.0/u);
});

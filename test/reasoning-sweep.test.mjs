import assert from "node:assert/strict";
import test from "node:test";
import { readFileSync } from "node:fs";
import { summarizeReasoningSweep } from "../src/reasoning-sweep.mjs";

function inputs() {
  const digest = "a".repeat(64);
  const phaseIds = ["candidate-compile", "public-test-compile", "public-tests"];
  const outcomes = ["answer", "timeout", "provider-error"];
  return {
    phaseIds,
    cohort: {
      plan: {
        taskId: "example", promptSha256: digest,
        models: [{ modelName: "model.reasoning-low", family: "same-family", runs: [1, 2, 3] }],
      },
      attempts: outcomes.map((outcome, index) => ({
        modelName: "model.reasoning-low", run: index + 1, outcome, resultSha256: digest,
      })),
    },
    evidence: outcomes.map((outcome, index) => ({
      modelName: "model.reasoning-low", run: index + 1, outcome,
      resultSha256: digest, durationMs: [10000, 600000, 2000][index],
      extraction: index === 0 ? { success: true, sha256: digest } : null,
      reportSha256: index === 0 ? digest : null,
      report: index === 0 ? {
        taskId: "example", answerSha256: digest, success: false,
        validationProfile: "c11-host", validationProfileRevision: 4,
        validationProfileSha256: digest,
        validationEnvironment: { id: "test-env", revision: 1, sha256: digest },
        phases: phaseIds.map((id, i) => ({
          id, phase: i === 2 ? "test" : "compile", outcome: i === 2 ? "failed" : "passed",
          stderr: "private diagnostic must not escape",
        })),
      } : null,
    })),
  };
}

test("sweep retains quota failures and timeouts in planned denominators and attempt latency", () => {
  const result = summarizeReasoningSweep(inputs());
  const group = result.groups[0];
  assert.equal(group.scheduled, 3);
  assert.equal(group.answers, 1);
  assert.equal(group.outcomes.timeout, 1);
  assert.equal(group.outcomes["provider-error"], 1);
  assert.equal(group.compiled, 1);
  assert.equal(group.passed, 0);
  assert.equal(group.meanAttemptSeconds, 204);
  assert.equal(group.meanAnswerSeconds, 10);
  assert.equal(result.plannedFamilyCount, 1);
  assert.equal(result.attempts[0].validationOutcome, "runtime-failure");
  assert.doesNotMatch(JSON.stringify(result), /private diagnostic|stderr|score/u);
});

test("sweep distinguishes compilation, extraction, and generation failures", () => {
  const input = inputs();
  const answer = input.evidence[0];
  answer.report.phases = [{ id: "candidate-compile", phase: "compile", outcome: "failed" }];
  assert.equal(summarizeReasoningSweep(input).attempts[0].validationOutcome, "compile-failure");
  answer.extraction = { success: false };
  answer.report = null;
  answer.reportSha256 = null;
  assert.equal(summarizeReasoningSweep(input).attempts[0].validationOutcome, "extraction-failure");
  input.cohort.attempts[0].outcome = answer.outcome = "no-answer";
  answer.extraction = null;
  const result = summarizeReasoningSweep(input);
  assert.equal(result.groups[0].meanAnswerSeconds, null);
  assert.equal(result.groups[0].answers, 0);
});

test("full passes require every declared fixture phase", () => {
  const input = inputs();
  input.evidence[0].report.success = true;
  assert.throws(() => summarizeReasoningSweep(input), /inconsistent validation/u);
  input.evidence[0].report.phases[2].outcome = "passed";
  assert.equal(summarizeReasoningSweep(input).groups[0].passed, 1);
  input.evidence[0].report.phases.pop();
  assert.throws(() => summarizeReasoningSweep(input), /incomplete or inconsistent/u);
});

for (const [name, mutate, pattern] of [
  ["missing attempts", (x) => { x.cohort.attempts[2].outcome = "missing"; }, /missing or unplanned/u],
  ["duplicate evidence", (x) => { x.evidence[2] = x.evidence[1]; }, /duplicate or incomplete/u],
  ["omitted evidence", (x) => { x.evidence.pop(); }, /duplicate or incomplete/u],
  ["changed raw result", (x) => { x.evidence[0].resultSha256 = "b".repeat(64); }, /original attempt/u],
  ["negative duration", (x) => { x.evidence[0].durationMs = -1; }, /original attempt/u],
  ["different answer", (x) => { x.evidence[0].report.answerSha256 = "b".repeat(64); }, /extracted answer/u],
  ["missing report", (x) => { x.evidence[0].report = null; }, /extracted answer/u],
  ["fabricated failure validation", (x) => { x.evidence[1].report = x.evidence[0].report; }, /failed generation/u],
  ["reordered phases", (x) => { x.evidence[0].report.phases.reverse(); }, /phase sequence/u],
]) {
  test(`sweep rejects ${name}`, () => {
    const input = inputs();
    mutate(input);
    assert.throws(() => summarizeReasoningSweep(input), pattern);
  });
}

test("sweep rejects mixed validation environments", () => {
  const input = inputs();
  input.cohort.attempts[1].outcome = "answer";
  input.evidence[1] = { ...structuredClone(input.evidence[0]), run: 2 };
  input.evidence[1].report.validationEnvironment.revision++;
  assert.throws(() => summarizeReasoningSweep(input), /environment/u);
});

test("published Luna sweep retains all scheduled failures and reproducible aggregates", () => {
  const summary = JSON.parse(readFileSync(new URL(
    "../docs/calibration/supervised-process-service-luna-reasoning-2026-09-08.json",
    import.meta.url,
  ), "utf8"));
  assert.equal(summary.attempts.length, 9);
  assert.equal(summary.plannedFamilyCount, 1);
  assert.equal(summary.methodology.rubricScoring, "not-performed");
  const identities = summary.attempts.map((row) => `${row.modelName}:${row.run}`);
  assert.equal(new Set(identities).size, 9);
  for (const group of summary.groups) {
    const rows = summary.attempts.filter((row) => row.modelName === group.modelName);
    const answers = rows.filter((row) => row.generationOutcome === "answer");
    assert.equal(group.scheduled, rows.length);
    assert.equal(group.answers, answers.length);
    assert.equal(group.passed, rows.filter((row) => row.validationOutcome === "passed").length);
    for (const field of ["extracted", "candidateCompiled", "compiled"]) {
      assert.equal(group[field], rows.filter((row) => row[field]).length);
    }
    assert.equal(group.meanAttemptSeconds, rows.reduce((sum, row) => sum + row.durationMs, 0) / rows.length / 1000);
    assert.equal(group.meanAnswerSeconds, answers.reduce((sum, row) => sum + row.durationMs, 0) / answers.length / 1000);
    for (const [outcome, count] of Object.entries(group.outcomes)) {
      assert.equal(count, rows.filter((row) => row.generationOutcome === outcome).length);
    }
    assert.equal(rows.find((row) => row.run === 3).generationOutcome, "provider-error");
  }
});

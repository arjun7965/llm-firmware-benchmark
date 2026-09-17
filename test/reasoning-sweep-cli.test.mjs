import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import test from "node:test";
import { sha256 } from "../src/fixture-answer-digests.mjs";

const script = fileURLToPath(new URL("../scripts/summarize-reasoning-sweep.mjs", import.meta.url));

function setup(t) {
  const cwd = mkdtempSync(join(tmpdir(), "reasoning-sweep-"));
  t.after(() => rmSync(cwd, { force: true, recursive: true }));
  const root = join(cwd, "results", "sweep");
  const write = (path, value) => {
    const target = join(root, path);
    mkdirSync(dirname(target), { recursive: true });
    const text = typeof value === "string" ? value : `${JSON.stringify(value)}\n`;
    writeFileSync(target, text);
    return sha256(text);
  };
  const task = {
    id: "example", prompt: "Implement the supplied API.", category: "test",
    suite: "firmware", scoringMode: "deterministic", validationProfile: "c11-host",
    targetProfile: "embedded-linux-posix",
  };
  const model = {
    modelName: "example.reasoning-low", modelId: "example", provider: "codex",
    family: "one-family", modelOptions: { effort: "low", timeoutMs: 600000 },
    providerConfigSha256: null, runs: [1],
  };
  const files = {};
  files["cohort.json"] = write("cohort.json", {
    schemaVersion: "1.0", taskId: task.id, promptSha256: sha256(task.prompt), models: [model],
  });
  write("cohort.sha256", files["cohort.json"]);
  files["tasks.json"] = write("tasks.json", [task]);
  files["plan.json"] = write("plan.json", { scheduledAttempts: 1 });
  const manifestHash = write("fixture-snapshot/example/manifest.json", {
    answer: { format: "markdown-fenced-code", language: "c" },
    commands: [{ id: "compile", phase: "compile" }, { id: "test", phase: "test" }],
  });
  files["fixture-hashes.json"] = write("fixture-hashes.json", {
    "fixtures/example/manifest.json": manifestHash,
  });
  write("provenance.json", { files });
  const answer = "int implementation(void) { return 0; }\n";
  write("answer.c", answer);
  const raw = {
    ...model, task: task.id, run: 1, promptSha256: sha256(task.prompt),
    category: task.category, suite: task.suite, scoringMode: task.scoringMode,
    validationProfile: task.validationProfile, targetProfile: task.targetProfile,
    startedAt: "2026-09-09T00:00:00Z", finishedAt: "2026-09-09T00:00:10Z",
    stdout: `\`\`\`c\n${answer}\`\`\``, stderr: "private data", exitCode: 0, error: null, signal: null,
  };
  const resultSha256 = write("raw/example--example.reasoning-low.json", raw);
  write("report.json", {
    taskId: task.id, answerSha256: sha256(answer), success: true,
    validationProfile: "c11-host", validationProfileRevision: 4,
    validationProfileSha256: "a".repeat(64),
    validationEnvironment: { id: "test-environment", revision: 1, sha256: "b".repeat(64) },
    phases: [
      { id: "compile", phase: "compile", outcome: "passed" },
      { id: "test", phase: "test", outcome: "passed" },
    ],
  });
  const audits = [{
    modelName: model.modelName, run: 1, outcome: "answer", durationMs: 10000, resultSha256,
    extraction: { success: true, outputPath: join(root, "answer.c"), sha256: sha256(answer) },
    validation: { success: true, reportPath: join(root, "report.json") },
  }];
  write("validation-audit.json", audits);
  const run = () => spawnSync(process.execPath, [
    script, "--directory", "results/sweep", "--output", "results/sweep/summary.json",
  ], { cwd, encoding: "utf8" });
  return { root, write, audits, run };
}

test("summary CLI verifies private evidence and refuses to overwrite its output", (t) => {
  const { root, run } = setup(t);
  const first = run();
  assert.equal(first.status, 0, first.stderr);
  const text = readFileSync(join(root, "summary.json"), "utf8");
  assert.equal(JSON.parse(text).groups[0].passed, 1);
  assert.doesNotMatch(text, /private data|outputPath|reportPath|int example/u);
  assert.notEqual(run().status, 0);
  assert.equal(readFileSync(join(root, "summary.json"), "utf8"), text);
});

for (const [name, change, expected] of [
  ["changed fixture", ({ write }) => write("fixture-snapshot/example/manifest.json", {}), /frozen evidence changed/u],
  ["changed duration", ({ write, audits }) => {
    audits[0].durationMs = 5;
    write("validation-audit.json", audits);
  }, /duration does not match/u],
  ["repaired answer", ({ write, audits }) => {
    audits[0].extraction.sha256 = write("answer.c", "repaired implementation\n");
    write("validation-audit.json", audits);
  }, /does not match raw answer/u],
  ["escaping report", ({ write, audits }) => {
    audits[0].validation.reportPath = "../outside.json";
    write("validation-audit.json", audits);
  }, /inside the cohort directory/u],
]) {
  test(`summary CLI rejects ${name}`, (t) => {
    const context = setup(t);
    change(context);
    const result = context.run();
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, expected);
  });
}

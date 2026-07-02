import {
  mkdtempSync,
  mkdirSync,
  readFileSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import assert from "node:assert/strict";
import {
  createJobs,
  executeJob,
  hasSuccessfulResult,
  mapWithConcurrency,
  promptSha256,
  resultFilePath,
} from "../src/harness.mjs";

const tasks = [
  { id: "task-one", category: "test", prompt: "First prompt" },
  { id: "task-two", category: "test", prompt: "Second prompt" },
];
const models = [
  { id: "alpha", provider: "fake", model: "model-a" },
  { id: "beta", provider: "fake", model: "model-b" },
];

function temporaryDirectory(t) {
  const path = mkdtempSync(join(tmpdir(), "llm-benchmark-test-"));
  t.after(() => rmSync(path, { recursive: true, force: true }));
  return path;
}

test("job generation creates the complete task/model/run product", () => {
  const jobs = createJobs(tasks, models, [1, 3]);

  assert.equal(jobs.length, 8);
  assert.deepEqual(
    jobs.map(({ run, task, modelName }) => [run, task.id, modelName]),
    [
      [1, "task-one", "alpha"],
      [1, "task-one", "beta"],
      [1, "task-two", "alpha"],
      [1, "task-two", "beta"],
      [3, "task-one", "alpha"],
      [3, "task-one", "beta"],
      [3, "task-two", "alpha"],
      [3, "task-two", "beta"],
    ],
  );
});

test("result paths preserve the existing run directory convention", () => {
  const [job] = createJobs(tasks, models);

  assert.equal(
    resultFilePath("/results", job),
    join("/results", "task-one--alpha.json"),
  );
  assert.equal(
    resultFilePath("/results", { ...job, run: 2 }),
    join("/results", "run-2", "task-one--alpha.json"),
  );
});

test("successful results are reusable while failures and malformed files are not", (t) => {
  const root = temporaryDirectory(t);
  const success = join(root, "success.json");
  const failure = join(root, "failure.json");
  const malformed = join(root, "malformed.json");
  writeFileSync(success, JSON.stringify({ exitCode: 0 }));
  writeFileSync(failure, JSON.stringify({ exitCode: 1 }));
  writeFileSync(malformed, "{");

  assert.equal(hasSuccessfulResult(success), true);
  assert.equal(hasSuccessfulResult(success, {
    expectedPromptSha256: "different",
  }), false);
  assert.equal(hasSuccessfulResult(failure), false);
  assert.equal(hasSuccessfulResult(malformed), false);
  assert.equal(hasSuccessfulResult(join(root, "missing.json")), false);
});

test("concurrency is bounded and result ordering is stable", async () => {
  let active = 0;
  let maximumActive = 0;
  const values = Array.from({ length: 9 }, (_, index) => index);

  const results = await mapWithConcurrency(values, 3, async (value) => {
    active++;
    maximumActive = Math.max(maximumActive, active);
    await new Promise((resolve) => setImmediate(resolve));
    active--;
    return value * 2;
  });

  assert.equal(maximumActive, 3);
  assert.deepEqual(results, values.map((value) => value * 2));
});

test("executeJob persists provider output and result metadata", async (t) => {
  const outputRoot = temporaryDirectory(t);
  const [job] = createJobs([
    {
      ...tasks[0],
      targetProfile: "portable-c11",
    },
  ], [models[0]]);
  let generatedJob;
  const generate = async (receivedJob) => {
    generatedJob = receivedJob;
    return {
      exitCode: 0,
      signal: null,
      stdout: "generated answer",
      stderr: "diagnostic",
      error: null,
    };
  };

  const result = await executeJob({
    job,
    outputRoot,
    generate,
    now: () => new Date("2026-01-01T00:00:00.000Z"),
  });
  const persisted = JSON.parse(readFileSync(result.path, "utf8"));

  assert.equal(result.status, "completed");
  assert.equal(generatedJob, job);
  assert.equal(persisted.exitCode, 0);
  assert.equal(persisted.stdout, "generated answer");
  assert.equal(persisted.stderr, "diagnostic");
  assert.equal(persisted.task, "task-one");
  assert.equal(persisted.targetProfile, "portable-c11");
  assert.equal(persisted.provider, "fake");
  assert.equal(persisted.modelName, "alpha");
  assert.equal(persisted.promptSha256, promptSha256(job.task.prompt));
  assert.deepEqual(persisted.modelOptions, {});
});

test("executeJob skips an existing successful result without generating", async (t) => {
  const outputRoot = temporaryDirectory(t);
  const [job] = createJobs(tasks, [models[0]]);
  const path = resultFilePath(outputRoot, job);
  mkdirSync(outputRoot, { recursive: true });
  writeFileSync(path, JSON.stringify({
    exitCode: 0,
    promptSha256: promptSha256(job.task.prompt),
  }));

  const result = await executeJob({
    job,
    outputRoot,
    generate: () => {
      throw new Error("must not generate");
    },
  });

  assert.deepEqual(result, { status: "skipped", path });

  writeFileSync(path, JSON.stringify({
    exitCode: 0,
    promptSha256: "0".repeat(64),
  }));
  const refreshed = await executeJob({
    job,
    outputRoot,
    generate: async () => ({
      exitCode: 0,
      signal: null,
      stdout: "refreshed",
      stderr: "",
      error: null,
    }),
  });
  assert.equal(refreshed.status, "completed");
  assert.equal(refreshed.record.stdout, "refreshed");
});

test("executeJob records rejected provider executions", async (t) => {
  const outputRoot = temporaryDirectory(t);
  const [job] = createJobs(tasks, [models[0]]);
  const result = await executeJob({
    job,
    outputRoot,
    generate: async () => {
      throw new Error("provider failed");
    },
  });

  assert.equal(result.record.exitCode, null);
  assert.equal(result.record.error, "provider failed");
});

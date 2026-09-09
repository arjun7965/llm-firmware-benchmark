import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { createJobs, executeJob } from "../src/harness.mjs";
import { buildCodexInvocation } from "../src/providers/codex.mjs";
import {
  benchmarkHelp,
  expandReasoningModels,
  filterByIds,
  filterBySuites,
  parseBenchmarkArgs,
  runBenchmarkCli,
} from "../src/benchmark-cli.mjs";

test("benchmark CLI parses filters and execution controls", () => {
  const configuration = parseBenchmarkArgs([
    "--models", "alpha,beta",
    "--tasks", "task-one",
    "--tasks", "task-two",
    "--suites", "firmware",
    "--runs", "1,3",
    "--concurrency", "2",
    "--output", "artifacts",
    "--models-file", "models.test.json",
    "--tasks-file", "tasks.test.json",
  ], {
    cwd: "/workspace",
    environment: {},
  });

  assert.deepEqual(configuration, {
    concurrency: 2,
    help: false,
    modelIds: ["alpha", "beta"],
    modelsFile: "/workspace/models.test.json",
    outputRoot: "/workspace/artifacts",
    reasoning: [],
    runs: [1, 3],
    suiteIds: ["firmware"],
    taskIds: ["task-one", "task-two"],
    tasksFile: "/workspace/tasks.test.json",
  });
});

test("benchmark CLI preserves mode defaults and environment model path", () => {
  const configuration = parseBenchmarkArgs([], {
    cwd: "/workspace",
    defaultRuns: [2, 3],
    environment: {
      BENCHMARK_MODELS_FILE: "private-models.json",
    },
  });

  assert.equal(configuration.concurrency, 4);
  assert.deepEqual(configuration.runs, [2, 3]);
  assert.equal(configuration.modelsFile, "/workspace/private-models.json");
  assert.equal(configuration.modelIds, null);
  assert.equal(configuration.suiteIds, null);
  assert.equal(configuration.taskIds, null);
});

test("benchmark CLI rejects invalid and duplicate values", () => {
  assert.throws(
    () => parseBenchmarkArgs(["--concurrency", "0"]),
    /positive integer/,
  );
  assert.throws(
    () => parseBenchmarkArgs(["--runs", "1,two"]),
    /positive integer/,
  );
  assert.throws(
    () => parseBenchmarkArgs(["--models", "alpha,alpha"]),
    /duplicates/,
  );
  assert.throws(
    () => parseBenchmarkArgs(["--runs", "999999999999999999999"]),
    /safe positive integer/,
  );
  assert.throws(
    () => parseBenchmarkArgs(["--suites", "unknown"]),
    /unknown suites/,
  );
  assert.throws(
    () => parseBenchmarkArgs(["--suites", "firmware,firmware"]),
    /duplicates/,
  );
});

test("suite filters preserve task order and require a match", () => {
  const tasks = [
    { id: "firmware-one", suite: "firmware" },
    { id: "auxiliary-one", suite: "auxiliary" },
    { id: "firmware-two", suite: "firmware" },
  ];

  assert.deepEqual(
    filterBySuites(tasks, ["firmware"]),
    [tasks[0], tasks[2]],
  );
  assert.deepEqual(filterBySuites(tasks, null), tasks);
  assert.throws(
    () => filterBySuites(tasks, ["missing"]),
    /no tasks matched suites/,
  );
});

test("ID filters preserve requested order and reject unknown IDs", () => {
  const items = [{ id: "alpha" }, { id: "beta" }];

  assert.deepEqual(
    filterByIds(items, ["beta", "alpha"], "model IDs"),
    [items[1], items[0]],
  );
  assert.throws(
    () => filterByIds(items, ["missing"], "model IDs"),
    /unknown model IDs: missing/,
  );
});

test("benchmark help documents all extensibility controls", () => {
  const help = benchmarkHelp([2, 3], "benchmark:repeats");

  for (const option of [
    "--models",
    "--suites",
    "--tasks",
    "--runs",
    "--concurrency",
    "--output",
    "--models-file",
    "--tasks-file",
    "--reasoning",
  ]) {
    assert.match(help, new RegExp(option));
  }
  assert.match(help, /default: 2,3/);
  assert.match(help, /npm run benchmark:repeats/);
});

test("reasoning selections expand provider controls without mutating model configuration", () => {
  const models = [
    { id: "gpt", provider: "codex", model: "gpt-model", options: { effort: "medium", timeoutMs: 1234 } },
    { id: "claude", provider: "claude-code", model: "claude-model" },
    { id: "kimi", provider: "opencode", model: "provider/kimi", options: { maxOutputTokens: 64000 } },
    { id: "local", provider: "openai-compatible", model: "local", options: { request: { temperature: 0, reasoning_effort: "low" } } },
    { id: "unchanged", provider: "codex", model: "other" },
  ];
  const original = structuredClone(models);
  const { reasoning } = parseBenchmarkArgs([
    "--reasoning", "gpt=low,high",
    "--reasoning", "gpt=max",
    "--reasoning", "claude=high",
    "--reasoning", "kimi=max",
    "--reasoning", "local=high",
  ]);
  const expanded = expandReasoningModels(models, reasoning);
  assert.deepEqual(expanded.map((m) => m.id), [
    "gpt.reasoning-low", "gpt.reasoning-high", "gpt.reasoning-max",
    "claude.reasoning-high", "kimi.reasoning-max", "local.reasoning-high", "unchanged",
  ]);
  assert.equal(expanded[0].model, "gpt-model");
  assert.deepEqual(expanded[0].options, { effort: "low", timeoutMs: 1234 });
  assert.equal(expanded[3].options.effort, "high");
  assert.deepEqual(expanded[4].options, { maxOutputTokens: 64000, variant: "max" });
  assert.deepEqual(expanded[5].options.request, { temperature: 0, reasoning_effort: "high" });
  assert.deepEqual(models, original);
});

test("reasoning rejects malformed selections, unsupported controls and result ID collisions", () => {
  for (const value of ["high", "gpt=", "gpt=low,", "gpt=../high", "gpt=low,low"]) {
    assert.throws(() => parseBenchmarkArgs(["--reasoning", value]), /reasoning/);
  }
  assert.throws(() => parseBenchmarkArgs([
    "--reasoning", "gpt=low", "--reasoning", "gpt=low",
  ]), /duplicate reasoning/);
  const model = { id: "gpt", provider: "codex", model: "gpt-model" };
  assert.throws(() => expandReasoningModels([model], [
    { modelId: "absent", levels: ["high"] },
  ]), /not selected/);
  for (const provider of ["codex", "claude-code", "unknown"]) {
    assert.throws(() => expandReasoningModels([{ ...model, provider }], [
      { modelId: "gpt", levels: ["made-up"] },
    ]), /unsupported/);
  }
  assert.throws(() => expandReasoningModels([
    model, { ...model, id: "gpt.reasoning-high" },
  ], [{ modelId: "gpt", levels: ["high"] }]), /duplicate model id/);
});

test("reasoning sweep smoke preserves prompts, provider invocation, metadata and separate results", async () => {
  const outputRoot = mkdtempSync(join(tmpdir(), "benchmark-reasoning-"));
  try {
    const configuration = parseBenchmarkArgs([
      "--reasoning", "gpt=low,high", "--runs", "1,2,3",
    ]);
    const models = expandReasoningModels([
      { id: "gpt", provider: "codex", model: "gpt-model" },
    ], configuration.reasoning);
    const task = { id: "test-task", category: "test", suite: "auxiliary",
      scoringMode: "deterministic", validationProfile: "python3-stdlib", prompt: "Same prompt" };
    const jobs = createJobs([task], models, configuration.runs);
    assert.equal(jobs.length, 6);
    const paths = new Set();
    for (const job of jobs) {
      const result = await executeJob({ job, outputRoot, generate: async (received) => {
        assert.equal(received.task.prompt, task.prompt);
        const invocation = buildCodexInvocation(received, { cwd: outputRoot });
        assert(invocation.args.some((arg) => arg.includes(`"${received.modelOptions.effort}"`)));
        return { exitCode: 0, signal: null, stdout: "mock answer", stderr: "" };
      } });
      const record = JSON.parse(readFileSync(result.path, "utf8"));
      assert.equal(record.exitCode, 0, record.error);
      assert.equal(record.generationBudget.reasoning.value, job.modelOptions.effort);
      assert.equal(record.generationBudget.reasoning.control, "effort");
      assert.equal(record.modelOptions.effort, job.modelOptions.effort);
      paths.add(result.path);
    }
    assert.equal(paths.size, 6);
  } finally {
    rmSync(outputRoot, { recursive: true, force: true });
  }
});

test("reasoning CLI rejects collisions with filtered-out models before writing results", async () => {
  const root = mkdtempSync(join(tmpdir(), "benchmark-reasoning-collision-"));
  try {
    const modelsFile = join(root, "models.json");
    const tasksFile = join(root, "tasks.json");
    const output = join(root, "output");
    writeFileSync(modelsFile, JSON.stringify({ models: [
      { id: "gpt", provider: "codex", model: "first-model" },
      { id: "gpt.reasoning-high", provider: "codex", model: "other-model" },
    ] }));
    writeFileSync(tasksFile, JSON.stringify([{
      id: "test-task", category: "test", suite: "auxiliary", scoringMode: "deterministic",
      validationProfile: "python3-stdlib", prompt: "Same prompt",
    }]));
    await assert.rejects(runBenchmarkCli({ args: [
      "--models-file", modelsFile, "--tasks-file", tasksFile,
      "--models", "gpt", "--reasoning", "gpt=high", "--output", output,
    ] }), /conflicts with the model catalog/);
    assert.equal(existsSync(output), false);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("reasoning override does not mask malformed compatible request configuration", () => {
  for (const request of ["invalid", 12, false, []]) {
    assert.throws(() => expandReasoningModels([
      { id: "local", provider: "openai-compatible", model: "local", options: { request } },
    ], [{ modelId: "local", levels: ["high"] }]), /request must be an object/);
  }
});

import test from "node:test";
import assert from "node:assert/strict";
import {
  benchmarkHelp,
  filterByIds,
  parseBenchmarkArgs,
} from "../src/benchmark-cli.mjs";

test("benchmark CLI parses filters and execution controls", () => {
  const configuration = parseBenchmarkArgs([
    "--models", "alpha,beta",
    "--tasks", "task-one",
    "--tasks", "task-two",
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
    runs: [1, 3],
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
    "--tasks",
    "--runs",
    "--concurrency",
    "--output",
    "--models-file",
    "--tasks-file",
  ]) {
    assert.match(help, new RegExp(option));
  }
  assert.match(help, /default: 2,3/);
  assert.match(help, /npm run benchmark:repeats/);
});

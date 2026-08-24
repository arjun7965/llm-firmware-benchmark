import assert from "node:assert/strict";
import {
  mkdtempSync,
  readFileSync,
  rmSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import test from "node:test";
import {
  c11OciValidationEnvironment,
  discoverC11CalibrationTasks,
  runOciC11Calibration,
  stageOciCalibrationCandidate,
  validateOciCalibrationReport,
} from "../src/oci-calibration.mjs";

const repositoryRoot = fileURLToPath(new URL("../", import.meta.url));
const fixturesRoot = join(repositoryRoot, "fixtures");
const tasksPath = join(repositoryRoot, "tasks.json");

function temporaryDirectory(t) {
  const path = mkdtempSync(join(tmpdir(), "oci-calibration-test-"));
  t.after(() => rmSync(path, { recursive: true, force: true }));
  return path;
}

function report({ mutation = false } = {}) {
  return {
    taskId: "binary-parser",
    success: !mutation,
    validationEnvironment: {
      id: c11OciValidationEnvironment,
      execution: { kind: "oci" },
    },
    phases: [
      {
        id: "host-compile",
        phase: "compile",
        outcome: "passed",
        timedOut: false,
        error: null,
        stdout: "",
        stderr: "",
      },
      {
        id: "public-tests",
        phase: "test",
        outcome: mutation ? "failed" : "passed",
        timedOut: false,
        error: null,
        stdout: "",
        stderr: "",
      },
    ],
  };
}

test("OCI C11 calibration covers every active C11 mutation catalog", () => {
  const tasks = discoverC11CalibrationTasks({ fixturesRoot, tasksPath });
  assert.equal(tasks.length, 41);
  assert.equal(
    tasks.reduce((total, task) => total + task.mutationCount, 0),
    595,
  );
  assert.equal(new Set(tasks.map(({ taskId }) => taskId)).size, tasks.length);
});

test("OCI calibration stages trusted references and exact mutations", (t) => {
  const destinationFixturesRoot = temporaryDirectory(t);
  const baseline = stageOciCalibrationCandidate({
    destinationFixturesRoot,
    fixturesRoot,
    taskId: "binary-parser",
  });
  const reference = readFileSync(
    join(fixturesRoot, "binary-parser/reference/binary_parser.c"),
    "utf8",
  );
  assert.equal(
    readFileSync(
      join(baseline.destinationRoot, "generated/answer.c"),
      "utf8",
    ),
    reference,
  );

  const catalog = JSON.parse(readFileSync(
    join(fixturesRoot, "binary-parser/mutations.json"),
    "utf8",
  ));
  const mutation = catalog.mutations[0];
  const staged = stageOciCalibrationCandidate({
    destinationFixturesRoot,
    fixturesRoot,
    mutationId: mutation.id,
    taskId: "binary-parser",
  });
  const candidate = readFileSync(
    join(staged.destinationRoot, "generated/answer.c"),
    "utf8",
  );
  assert.ok(candidate.includes(mutation.replace));
  assert.equal(candidate.includes(mutation.find), false);
});

test("OCI calibration requires passing baselines and compile-valid kills", () => {
  const baseline = report();
  assert.equal(validateOciCalibrationReport(baseline), baseline);
  const killed = report({ mutation: true });
  assert.equal(
    validateOciCalibrationReport(killed, { mutationId: "skip-bounds" }),
    killed,
  );

  const compileFailure = report({ mutation: true });
  compileFailure.phases[0].outcome = "failed";
  assert.throws(
    () => validateOciCalibrationReport(compileFailure, {
      mutationId: "skip-bounds",
    }),
    /not compile-valid and killed/u,
  );
  const survived = report();
  assert.throws(
    () => validateOciCalibrationReport(survived, {
      mutationId: "skip-bounds",
    }),
    /not compile-valid and killed/u,
  );
});

test("OCI calibration shards fixtures and selects the OCI environment", (t) => {
  const tasks = discoverC11CalibrationTasks({ fixturesRoot, tasksPath });
  const shardIndex = tasks.findIndex(({ taskId }) => taskId === "binary-parser");
  const calls = [];
  const messages = [];
  const result = runOciC11Calibration({
    fixturesRoot,
    logger: (message) => messages.push(message),
    runFixtureValidationImpl: (options) => {
      calls.push(options);
      return { report: report({ mutation: calls.length > 1 }) };
    },
    shardCount: tasks.length,
    shardIndex,
    tasksPath,
    temporaryRoot: temporaryDirectory(t),
  });

  assert.deepEqual(result, {
    fixtureCount: 1,
    killedMutations: 5,
    shardCount: tasks.length,
    shardIndex,
  });
  assert.equal(calls.length, 6);
  assert.ok(calls.every((options) =>
    options.taskId === "binary-parser" &&
    options.validationEnvironmentId === c11OciValidationEnvironment));
  assert.match(messages.at(-1), /1 fixtures, 5 mutations killed/u);
});

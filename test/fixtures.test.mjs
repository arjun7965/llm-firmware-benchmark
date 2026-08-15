import {
  cpSync,
  mkdirSync,
  mkdtempSync,
  readFileSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import assert from "node:assert/strict";
import { loadTasks } from "../src/harness.mjs";
import {
  fixtureAnswerFiles,
  validateFixtureManifest,
  validateFixtureRepository,
} from "../src/fixtures.mjs";

const fixturesRoot = new URL("../fixtures/", import.meta.url);
const tasksPath = new URL("../tasks.json", import.meta.url);

function temporaryDirectory(t) {
  const path = mkdtempSync(join(tmpdir(), "fixture-policy-test-"));
  t.after(() => rmSync(path, { recursive: true, force: true }));
  return path;
}

function ringBufferFixture() {
  return JSON.parse(
    readFileSync(
      new URL(
        "../fixtures/embedded-ring-buffer/manifest.json",
        import.meta.url,
      ),
      "utf8",
    ),
  );
}

function concurrencyFixture() {
  return JSON.parse(
    readFileSync(
      new URL(
        "../fixtures/concurrency-debug/manifest.json",
        import.meta.url,
      ),
      "utf8",
    ),
  );
}

function goFixture() {
  return JSON.parse(
    readFileSync(
      new URL(
        "../fixtures/go-graceful-shutdown/manifest.json",
        import.meta.url,
      ),
      "utf8",
    ),
  );
}

test("repository fixture scaffolds match task metadata", () => {
  assert.deepEqual(
    validateFixtureRepository({ fixturesRoot, tasksPath }),
    {
      fixtureCount: 51,
      activeCount: 51,
      scaffoldCount: 0,
      commandCount: 108,
      firmwareHostCoverageCount: 42,
      firmwareMockAssetCount: 33,
      firmwareNoRuntimeMockCount: 9,
    },
  );
});

test("fixture validation rejects profile mismatch and unsafe paths", () => {
  const task = loadTasks(tasksPath)
    .find((item) => item.id === "embedded-ring-buffer");
  const manifest = ringBufferFixture();

  assert.equal(validateFixtureManifest(manifest, task), manifest);
  assert.throws(
    () => validateFixtureManifest({
      ...manifest,
      schemaVersion: "1.4",
      validationProfile: "cpp17-host",
    }, {
      ...task,
      validationProfile: "cpp17-host",
    }),
    /cpp17-host requires fixture schemaVersion 1\.5/u,
  );
  assert.throws(
    () => validateFixtureManifest({
      ...manifest,
      targetProfile: "portable-c11",
    }, task),
    /targetProfile does not match/,
  );
  assert.throws(
    () => validateFixtureManifest({
      ...manifest,
      validationProfile: "stable-rust",
    }, task),
    /validationProfile does not match/,
  );
  assert.throws(
    () => validateFixtureManifest({
      ...manifest,
      validationProfile: "node-typescript-postgresql",
    }, {
      ...task,
      validationProfile: "node-typescript-postgresql",
    }),
    /tool cc is not in its validation profile/u,
  );
  assert.throws(
    () => validateFixtureManifest({
      ...manifest,
      paths: {
        ...manifest.paths,
        starter: "../outside",
      },
    }, task),
    /safe relative path/,
  );
  assert.throws(
    () => validateFixtureManifest({
      ...manifest,
      answer: {
        ...manifest.answer,
        output: "starter/answer.c",
      },
    }, task),
    /under generated/,
  );
});

test("fixture eligibility follows the task scoring mode", (t) => {
  const task = loadTasks(tasksPath)
    .find((item) => item.id === "embedded-ring-buffer");
  const manifest = ringBufferFixture();

  assert.throws(
    () => validateFixtureManifest({ ...manifest }, {
      ...task,
      scoringMode: "rubric-only",
      rubricOnlyReasons: ["undocumented-service"],
      rubricOnlyRationale: "The required service cannot be reproduced.",
    }),
    /rubric-only task.*cannot define a fixture/u,
  );

  const root = temporaryDirectory(t);
  const fixturesRoot = join(root, "fixtures");
  const temporaryTasksPath = join(root, "tasks.json");
  mkdirSync(fixturesRoot);
  const baseTask = {
    id: "policy-task",
    category: "review",
    suite: "auxiliary",
    validationProfile: "python3-stdlib",
    prompt: "Review this answer.",
  };

  writeFileSync(temporaryTasksPath, JSON.stringify([{
    ...baseTask,
    scoringMode: "deterministic",
  }]));
  assert.throws(
    () => validateFixtureRepository({
      fixturesRoot,
      tasksPath: temporaryTasksPath,
    }),
    /policy-task is missing a fixture directory/u,
  );

  writeFileSync(temporaryTasksPath, JSON.stringify([{
    ...baseTask,
    scoringMode: "rubric-only",
    rubricOnlyReasons: ["environment-dependent-scoring"],
    rubricOnlyRationale: "The result depends on an unavailable customer state.",
  }]));
  assert.deepEqual(
    validateFixtureRepository({
      fixturesRoot,
      tasksPath: temporaryTasksPath,
    }),
    {
      fixtureCount: 0,
      activeCount: 0,
      scaffoldCount: 0,
      commandCount: 0,
      firmwareHostCoverageCount: 0,
      firmwareMockAssetCount: 0,
      firmwareNoRuntimeMockCount: 0,
    },
  );
});

test("firmware fixtures enforce documented host-side coverage", (t) => {
  const root = temporaryDirectory(t);
  const temporaryFixturesRoot = join(root, "fixtures");
  const temporaryTasksPath = join(root, "tasks.json");
  const taskIds = ["embedded-ring-buffer", "firmware-state-machine"];
  const tasks = loadTasks(tasksPath)
    .filter((task) => taskIds.includes(task.id));
  mkdirSync(temporaryFixturesRoot);
  for (const taskId of taskIds) {
    cpSync(
      new URL(`../fixtures/${taskId}/`, import.meta.url),
      join(temporaryFixturesRoot, taskId),
      { recursive: true },
    );
  }
  writeFileSync(temporaryTasksPath, JSON.stringify(tasks));

  assert.deepEqual(
    validateFixtureRepository({
      fixturesRoot: temporaryFixturesRoot,
      tasksPath: temporaryTasksPath,
    }),
    {
      fixtureCount: 2,
      activeCount: 2,
      scaffoldCount: 0,
      commandCount: 4,
      firmwareHostCoverageCount: 2,
      firmwareMockAssetCount: 1,
      firmwareNoRuntimeMockCount: 1,
    },
  );

  const stateMachineManifestPath = join(
    temporaryFixturesRoot,
    "firmware-state-machine",
    "manifest.json",
  );
  const stateMachineManifest = JSON.parse(
    readFileSync(stateMachineManifestPath, "utf8"),
  );
  writeFileSync(stateMachineManifestPath, JSON.stringify({
    ...stateMachineManifest,
    status: "scaffold",
  }));
  assert.throws(
    () => validateFixtureRepository({
      fixturesRoot: temporaryFixturesRoot,
      tasksPath: temporaryTasksPath,
    }),
    /firmware fixture firmware-state-machine must be active/u,
  );
  writeFileSync(stateMachineManifestPath, JSON.stringify(stateMachineManifest));

  const ringBufferMocks = join(
    temporaryFixturesRoot,
    "embedded-ring-buffer",
    "mocks",
  );
  const ringBufferReadme = join(ringBufferMocks, "README.md");
  const readme = readFileSync(ringBufferReadme, "utf8");
  writeFileSync(ringBufferReadme, "\n");
  assert.throws(
    () => validateFixtureRepository({
      fixturesRoot: temporaryFixturesRoot,
      tasksPath: temporaryTasksPath,
    }),
    /must document its host-side boundary in mocks\/README\.md/u,
  );
  writeFileSync(ringBufferReadme, readme);

  writeFileSync(join(ringBufferMocks, "unused_mock.c"), "int mock_value;\n");
  assert.throws(
    () => validateFixtureRepository({
      fixturesRoot: temporaryFixturesRoot,
      tasksPath: temporaryTasksPath,
    }),
    /mock assets are not used by its commands/u,
  );
});

test("fixture commands must match profile-approved runtime contracts", () => {
  const task = loadTasks(tasksPath)
    .find((item) => item.id === "concurrency-debug");
  const manifest = concurrencyFixture();

  assert.equal(validateFixtureManifest(manifest, task), manifest);
  assert.throws(
    () => validateFixtureManifest({
      ...manifest,
      commands: [
        manifest.commands[0],
        {
          ...manifest.commands[1],
          argv: ["python3", "tests/public/test_pool.py"],
        },
      ],
    }, task),
    /command public-tests is not approved by validation profile python3-stdlib/u,
  );
  assert.throws(
    () => validateFixtureManifest({
      ...manifest,
      commands: [
        manifest.commands[0],
        {
          ...manifest.commands[1],
          argv: ["build/public-tests"],
          requiredTools: [],
        },
      ],
    }, task),
    /command public-tests is not approved by validation profile python3-stdlib/u,
  );
  assert.equal(manifest.status, "active");
});

test("fixture bundles require sorted unique safe files", () => {
  const task = loadTasks(tasksPath)
    .find((item) => item.id === "go-graceful-shutdown");
  const manifest = goFixture();

  assert.equal(validateFixtureManifest(manifest, task), manifest);
  assert.deepEqual(fixtureAnswerFiles(manifest), [
    { language: "go", path: "generated/server.go" },
    { language: "go", path: "generated/server_test.go" },
  ]);
  assert.throws(
    () => validateFixtureManifest({
      ...manifest,
      answer: {
        ...manifest.answer,
        files: [...manifest.answer.files].reverse(),
      },
    }, task),
    /sorted and unique/u,
  );
  assert.throws(
    () => validateFixtureManifest({
      ...manifest,
      answer: {
        ...manifest.answer,
        files: [
          manifest.answer.files[0],
          { path: "../server_test.go", language: "go" },
        ],
      },
    }, task),
    /safe relative path/u,
  );
});

test("fixture commands cannot invoke a shell", () => {
  const task = loadTasks(tasksPath)
    .find((item) => item.id === "embedded-ring-buffer");
  const manifest = ringBufferFixture();
  const goManifest = {
    ...manifest,
    toolVersionArgs: {
      go: ["version"],
    },
    commands: [
      {
        ...manifest.commands[0],
        argv: ["go", "build"],
        requiredTools: ["go"],
      },
      manifest.commands[1],
    ],
  };

  assert.throws(
    () => validateFixtureManifest(goManifest, task),
    /tool go is not in its validation profile/u,
  );
  assert.throws(
    () => validateFixtureManifest({
      ...manifest,
      validationProfile: "stable-rust",
      toolVersionArgs: {
        rustc: ["--version"],
      },
      commands: [
        {
          ...manifest.commands[0],
          argv: ["rustc", "generated/answer.c"],
          requiredTools: ["rustc"],
        },
        manifest.commands[1],
      ],
    }, {
      ...task,
      validationProfile: "stable-rust",
    }),
    /requiredTools must cover.*toolchains exactly/u,
  );
  assert.throws(
    () => validateFixtureManifest({
      ...manifest,
      commands: [{
        ...manifest.commands[0],
        argv: ["sh", "-c", "cc generated/answer.c"],
        requiredTools: ["sh"],
      }],
    }, task),
    /declared non-shell tool/,
  );
  assert.throws(
    () => validateFixtureManifest({
      ...manifest,
      toolVersionArgs: {},
    }, task),
    /must cover requiredTools exactly/u,
  );
  assert.throws(
    () => validateFixtureManifest({
      ...manifest,
      toolVersionArgs: {
        cc: ["--version"],
        go: ["version"],
      },
    }, task),
    /tool go is not in its validation profile/u,
  );
  assert.throws(
    () => validateFixtureManifest({
      ...manifest,
      toolVersionArgs: {
        cc: ["--version\0"],
      },
    }, task),
    /toolVersionArgs is invalid/u,
  );
  assert.throws(
    () => validateFixtureManifest({
      ...manifest,
      commands: [{
        ...manifest.commands[0],
        requiredTools: [],
      }],
    }, task),
    /declared non-shell tool/,
  );
  assert.throws(
    () => validateFixtureManifest({
      ...manifest,
      commands: [{
        id: "unsafe-test",
        phase: "test",
        argv: ["../outside"],
        requiredTools: [],
        timeoutMs: 1000,
      }],
    }, task),
    /safe relative path/,
  );
  assert.throws(
    () => validateFixtureManifest({
      ...manifest,
      commands: [
        {
          ...manifest.commands[0],
          phase: "analyze",
        },
        manifest.commands[1],
      ],
    }, task),
    /must define a compile command/,
  );
});

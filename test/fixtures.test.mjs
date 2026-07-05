import { readFileSync } from "node:fs";
import test from "node:test";
import assert from "node:assert/strict";
import { loadTasks } from "../src/harness.mjs";
import {
  validateFixtureManifest,
  validateFixtureRepository,
} from "../src/fixtures.mjs";

const fixturesRoot = new URL("../fixtures/", import.meta.url);
const tasksPath = new URL("../tasks.json", import.meta.url);

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

test("repository fixture scaffolds match task metadata", () => {
  assert.deepEqual(
    validateFixtureRepository({ fixturesRoot, tasksPath }),
    {
      fixtureCount: 4,
      activeCount: 4,
      scaffoldCount: 0,
      commandCount: 8,
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
      validationProfile: "node-typescript",
    }, {
      ...task,
      validationProfile: "node-typescript",
    }),
    /must remain a scaffold.*dependencies are unverifiable/u,
  );
  assert.throws(
    () => validateFixtureManifest({
      ...manifest,
      validationProfile: "python3-stdlib",
    }, {
      ...task,
      validationProfile: "python3-stdlib",
    }),
    /must remain a scaffold.*unavailable test runtime/u,
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

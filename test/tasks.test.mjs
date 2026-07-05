import { readFileSync } from "node:fs";
import test from "node:test";
import assert from "node:assert/strict";
import {
  loadTasks,
  validateTasks,
} from "../src/harness.mjs";
import { validationProfileIds } from "../src/validation-profiles.mjs";

test("repository tasks are valid and cover distinct categories", () => {
  const tasks = loadTasks(new URL("../tasks.json", import.meta.url));

  assert.ok(tasks.length >= 12);
  assert.equal(new Set(tasks.map((task) => task.id)).size, tasks.length);
  assert.ok(new Set(tasks.map((task) => task.category)).size >= 8);
  assert.equal(tasks.filter((task) => task.suite === "firmware").length, 4);
  assert.equal(tasks.filter((task) => task.suite === "auxiliary").length, 9);
  assert.deepEqual(
    [...new Set(tasks.map((task) => task.validationProfile))].sort(),
    validationProfileIds,
  );
  const profileDocumentation = readFileSync(
    new URL("../docs/validation-profiles.md", import.meta.url),
    "utf8",
  );
  for (const task of tasks) {
    assert.match(profileDocumentation, new RegExp(`\\\`${task.id}\\\``));
  }
});

test("task validation rejects duplicate and malformed IDs", () => {
  const valid = {
    id: "valid-task",
    category: "test",
    suite: "auxiliary",
    validationProfile: "python3-stdlib",
    prompt: "Do work.",
  };

  assert.throws(
    () => validateTasks([valid, { ...valid }]),
    /duplicate task id/,
  );
  assert.throws(
    () => validateTasks([{ ...valid, id: "Invalid_Task" }]),
    /invalid id/,
  );
});

test("task validation requires nonempty categories and prompts", () => {
  assert.throws(
    () => validateTasks([{ id: "missing-category", category: " ", prompt: "x" }]),
    /must have a category/,
  );
  assert.throws(
    () => validateTasks([{ id: "missing-prompt", category: "test", prompt: "" }]),
    /must have a prompt/,
  );
  assert.throws(
    () => validateTasks([{
      id: "missing-suite",
      category: "test",
      validationProfile: "python3-stdlib",
      prompt: "x",
    }]),
    /suite/,
  );
  assert.throws(
    () => validateTasks([{
      id: "invalid-suite",
      category: "test",
      suite: "primary",
      validationProfile: "python3-stdlib",
      prompt: "x",
    }]),
    /suite/,
  );
  assert.throws(
    () => validateTasks([{
      id: "extra-field",
      category: "test",
      suite: "auxiliary",
      validationProfile: "python3-stdlib",
      prompt: "x",
      answer: "not part of the task contract",
    }]),
    /unexpected fields/,
  );
});

test("embedded tasks require a known target profile", () => {
  const embeddedTask = {
    id: "embedded-task",
    category: "embedded",
    suite: "firmware",
    validationProfile: "c11-host",
    prompt: "Implement firmware.",
  };

  assert.throws(
    () => validateTasks([embeddedTask]),
    /must have a targetProfile/,
  );
  assert.throws(
    () => validateTasks([{
      ...embeddedTask,
      targetProfile: "unknown-profile",
    }]),
    /invalid targetProfile/,
  );
  assert.equal(
    validateTasks([{
      ...embeddedTask,
      targetProfile: "armv7m-bare-metal",
    }])[0].targetProfile,
    "armv7m-bare-metal",
  );
  assert.throws(
    () => validateTasks([{
      id: "auxiliary-profile",
      category: "test",
      suite: "auxiliary",
      validationProfile: "python3-stdlib",
      targetProfile: "armv7m-bare-metal",
      prompt: "Run a hosted test.",
    }]),
    /auxiliary task.*cannot have a targetProfile/,
  );
  assert.throws(
    () => validateTasks([{
      ...embeddedTask,
      suite: "auxiliary",
    }]),
    /must have a targetProfile|category must use the firmware suite/,
  );
});

test("tasks require a known validation profile", () => {
  const task = {
    id: "hosted-task",
    category: "test",
    suite: "auxiliary",
    prompt: "Run a hosted test.",
  };

  assert.throws(
    () => validateTasks([task]),
    /validationProfile/,
  );
  assert.throws(
    () => validateTasks([{
      ...task,
      validationProfile: "unknown-profile",
    }]),
    /validationProfile/,
  );
  assert.equal(
    validateTasks([{
      ...task,
      validationProfile: "python3-stdlib",
    }])[0].validationProfile,
    "python3-stdlib",
  );
});

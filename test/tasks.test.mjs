import test from "node:test";
import assert from "node:assert/strict";
import {
  loadTasks,
  validateTasks,
} from "../src/harness.mjs";

test("repository tasks are valid and cover distinct categories", () => {
  const tasks = loadTasks(new URL("../tasks.json", import.meta.url));

  assert.ok(tasks.length >= 12);
  assert.equal(new Set(tasks.map((task) => task.id)).size, tasks.length);
  assert.ok(new Set(tasks.map((task) => task.category)).size >= 8);
  assert.equal(tasks.filter((task) => task.suite === "firmware").length, 4);
  assert.equal(tasks.filter((task) => task.suite === "auxiliary").length, 9);
});

test("task validation rejects duplicate and malformed IDs", () => {
  const valid = {
    id: "valid-task",
    category: "test",
    suite: "auxiliary",
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
      prompt: "x",
    }]),
    /suite/,
  );
  assert.throws(
    () => validateTasks([{
      id: "invalid-suite",
      category: "test",
      suite: "primary",
      prompt: "x",
    }]),
    /suite/,
  );
  assert.throws(
    () => validateTasks([{
      id: "extra-field",
      category: "test",
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

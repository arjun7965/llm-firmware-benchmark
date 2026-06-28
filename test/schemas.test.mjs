import { readFileSync } from "node:fs";
import test from "node:test";
import assert from "node:assert/strict";
import { loadTasks } from "../src/harness.mjs";
import {
  loadScores,
  scoreModelIds,
  validateScores,
} from "../src/scores.mjs";

test("repository task and score documents match runtime contracts", () => {
  const tasks = loadTasks(new URL("../tasks.json", import.meta.url));
  const scores = loadScores(
    new URL("../repeat-scores.example.json", import.meta.url),
  );

  assert.ok(tasks.length > 0);
  assert.deepEqual(scoreModelIds(scores), ["model-a", "model-b"]);
});

test("score validation rejects malformed runs and out-of-range values", () => {
  const valid = {
    rubric: "Scores are out of 10",
    tasks: ["task-one"],
    model: {
      run1: [8],
    },
  };

  assert.equal(validateScores(valid), valid);
  assert.throws(
    () => validateScores({
      ...valid,
      tasks: ["task-one", "task-one"],
    }),
    /duplicates/,
  );
  assert.throws(
    () => validateScores({
      ...valid,
      model: { first: [8] },
    }),
    /invalid run name/,
  );
  assert.throws(
    () => validateScores({
      ...valid,
      model: { run1: [11] },
    }),
    /invalid scores/,
  );
  assert.throws(
    () => validateScores({
      ...valid,
      model: { run1: [] },
    }),
    /invalid scores/,
  );
});

test("JSON Schema files declare the expected contracts", () => {
  const taskSchema = JSON.parse(
    readFileSync(
      new URL("../schemas/tasks.schema.json", import.meta.url),
      "utf8",
    ),
  );
  const scoreSchema = JSON.parse(
    readFileSync(
      new URL("../schemas/repeat-scores.schema.json", import.meta.url),
      "utf8",
    ),
  );

  assert.equal(taskSchema.$schema, "https://json-schema.org/draft/2020-12/schema");
  assert.equal(taskSchema.items.additionalProperties, false);
  assert.equal(scoreSchema.$schema, taskSchema.$schema);
  assert.equal(scoreSchema.additionalProperties, false);
});

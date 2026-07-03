import { readFileSync } from "node:fs";
import test from "node:test";
import assert from "node:assert/strict";
import { loadTasks } from "../src/harness.mjs";
import { targetProfileIds } from "../src/target-profiles.mjs";
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
  const publicResultSchema = JSON.parse(
    readFileSync(
      new URL("../schemas/public-result.schema.json", import.meta.url),
      "utf8",
    ),
  );
  const fixtureSchema = JSON.parse(
    readFileSync(
      new URL("../schemas/fixture-manifest.schema.json", import.meta.url),
      "utf8",
    ),
  );
  const fixtureMutationsSchema = JSON.parse(
    readFileSync(
      new URL("../schemas/fixture-mutations.schema.json", import.meta.url),
      "utf8",
    ),
  );
  const fixtureValidationSchema = JSON.parse(
    readFileSync(
      new URL(
        "../schemas/fixture-validation-report.schema.json",
        import.meta.url,
      ),
      "utf8",
    ),
  );

  assert.equal(taskSchema.$schema, "https://json-schema.org/draft/2020-12/schema");
  assert.equal(taskSchema.items.additionalProperties, false);
  assert.deepEqual(
    taskSchema.items.properties.targetProfile.enum,
    targetProfileIds,
  );
  assert.deepEqual(
    taskSchema.items.allOf[0].then.required,
    ["targetProfile"],
  );
  assert.equal(scoreSchema.$schema, taskSchema.$schema);
  assert.equal(scoreSchema.additionalProperties, false);
  assert.deepEqual(
    publicResultSchema.properties.task.properties.targetProfile.enum,
    [null, ...targetProfileIds],
  );
  assert.deepEqual(
    fixtureSchema.properties.targetProfile.enum,
    [null, ...targetProfileIds],
  );
  assert.equal(fixtureSchema.additionalProperties, false);
  const commandCondition = fixtureSchema.properties.commands.items.allOf[0];
  assert.equal(
    commandCondition.if.properties.argv.prefixItems[0].pattern,
    "^build/",
  );
  assert.equal(commandCondition.then.properties.phase.const, "test");
  assert.equal(
    commandCondition.then.properties.requiredTools.maxItems,
    0,
  );
  assert.equal(
    commandCondition.else.properties.requiredTools.minItems,
    1,
  );
  assert.equal(fixtureMutationsSchema.$schema, taskSchema.$schema);
  assert.equal(fixtureMutationsSchema.additionalProperties, false);
  assert.equal(
    fixtureMutationsSchema.properties.mutations.items.additionalProperties,
    false,
  );
  assert.deepEqual(
    fixtureValidationSchema.properties.targetProfile.enum,
    [null, ...targetProfileIds],
  );
  assert.equal(fixtureValidationSchema.additionalProperties, false);
});

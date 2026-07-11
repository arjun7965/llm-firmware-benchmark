import { readFileSync } from "node:fs";
import test from "node:test";
import assert from "node:assert/strict";
import { loadTasks } from "../src/harness.mjs";
import { targetProfileIds } from "../src/target-profiles.mjs";
import {
  getValidationEnvironmentRevision,
  getValidationProfile,
  validationEnvironmentReference,
  validationProfileReference,
  validationProfileIds,
} from "../src/validation-profiles.mjs";
import {
  loadScores,
  scoreModelIds,
  validateScores,
} from "../src/scores.mjs";

test("repository task and score documents match runtime contracts", () => {
  const tasks = loadTasks(new URL("../tasks.json", import.meta.url));
  const scores = loadScores(
    new URL("../repeat-scores.example.json", import.meta.url),
    {
      validationProfileByTask: new Map(
        tasks.map((task) => [task.id, task.validationProfile]),
      ),
    },
  );

  assert.ok(tasks.length > 0);
  assert.deepEqual(scoreModelIds(scores), ["model-a", "model-b"]);
});

test("score validation rejects malformed runs and out-of-range values", () => {
  const valid = {
    schemaVersion: "1.0",
    rubric: "Scores are out of 10",
    validationContracts: {
      "task-one": {
        profile: validationProfileReference(
          getValidationProfile("c11-host"),
        ),
        environment: validationEnvironmentReference(
          getValidationEnvironmentRevision(
            "ubuntu-24-04-x86-64-c11-host",
            1,
          ),
        ),
      },
    },
    tasks: ["task-one"],
    model: {
      run1: [8],
    },
  };
  const options = {
    validationProfileByTask: new Map([["task-one", "c11-host"]]),
  };
  const validate = (scores) => validateScores(scores, options);

  assert.equal(validate(valid), valid);
  assert.throws(
    () => validate({
      ...valid,
      tasks: ["task-one", "task-one"],
    }),
    /duplicates/,
  );
  assert.throws(
    () => validate({
      ...valid,
      model: { first: [8] },
    }),
    /invalid run name/,
  );
  assert.throws(
    () => validate({
      ...valid,
      model: { run1: [11] },
    }),
    /invalid scores/,
  );
  assert.throws(
    () => validate({
      ...valid,
      model: { run1: [] },
    }),
    /invalid scores/,
  );
  assert.throws(
    () => validate({
      ...valid,
      validationContracts: {
        "task-one": {
          ...valid.validationContracts["task-one"],
          environment: {
            ...valid.validationContracts["task-one"].environment,
            sha256: "0".repeat(64),
          },
        },
      },
    }),
    /environmentSha256/u,
  );
  assert.throws(
    () => validate({
      ...valid,
      tasks: ["task-one", "task-two"],
    }),
    /must cover exactly/u,
  );
  assert.throws(
    () => validate({
      ...valid,
      validationContracts: {
        "task-one": {
          ...valid.validationContracts["task-one"],
          environment: validationEnvironmentReference(
            getValidationEnvironmentRevision(
              "ubuntu-24-04-x86-64-react18-typescript",
              1,
            ),
          ),
        },
      },
    }),
    /environment is unsupported/u,
  );
  assert.throws(
    () => validateScores(valid, {
      validationProfileByTask: new Map([
        ["task-one", "stable-rust"],
      ]),
    }),
    /profile does not match task/u,
  );
  assert.throws(
    () => validate({
      ...valid,
      validationContracts: {
        "task-one": {
          ...valid.validationContracts["task-one"],
          profile: {
            ...valid.validationContracts["task-one"].profile,
            sha256: "0".repeat(64),
          },
        },
      },
    }),
    /profileSha256/u,
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
  const validationProfilesSchema = JSON.parse(
    readFileSync(
      new URL(
        "../schemas/validation-profiles.schema.json",
        import.meta.url,
      ),
      "utf8",
    ),
  );

  assert.equal(taskSchema.$schema, "https://json-schema.org/draft/2020-12/schema");
  assert.equal(taskSchema.items.additionalProperties, false);
  assert.ok(taskSchema.items.required.includes("suite"));
  assert.deepEqual(
    taskSchema.items.properties.suite.enum,
    ["firmware", "auxiliary"],
  );
  assert.deepEqual(
    taskSchema.items.properties.targetProfile.enum,
    targetProfileIds,
  );
  assert.ok(taskSchema.items.required.includes("validationProfile"));
  assert.deepEqual(
    taskSchema.items.properties.validationProfile.enum,
    validationProfileIds,
  );
  assert.deepEqual(
    taskSchema.items.allOf[0].then.required,
    ["targetProfile"],
  );
  assert.equal(
    taskSchema.items.allOf[1].then.properties.suite.const,
    "firmware",
  );
  assert.equal(
    taskSchema.items.allOf[2].then.properties.suite.const,
    "firmware",
  );
  assert.equal(scoreSchema.$schema, taskSchema.$schema);
  assert.equal(scoreSchema.additionalProperties, false);
  assert.equal(scoreSchema.properties.schemaVersion.const, "1.0");
  assert.ok(scoreSchema.required.includes("validationContracts"));
  assert.deepEqual(
    publicResultSchema.properties.task.properties.targetProfile.enum,
    [null, ...targetProfileIds],
  );
  assert.deepEqual(
    publicResultSchema.properties.task.properties.validationProfile.enum,
    validationProfileIds,
  );
  assert.equal(publicResultSchema.properties.schemaVersion.const, "1.3");
  assert.deepEqual(
    publicResultSchema.properties.task.properties.suite.enum,
    ["firmware", "auxiliary"],
  );
  assert.equal(
    publicResultSchema.properties.task.allOf[1]
      .then.properties.targetProfile.const,
    null,
  );
  assert.deepEqual(
    fixtureSchema.properties.targetProfile.enum,
    [null, ...targetProfileIds],
  );
  assert.deepEqual(
    fixtureSchema.properties.validationProfile.enum,
    validationProfileIds,
  );
  assert.equal(fixtureSchema.properties.schemaVersion.const, "1.3");
  assert.equal(
    fixtureSchema.properties.toolVersionArgs.additionalProperties.minItems,
    1,
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
  assert.equal(
    fixtureMutationsSchema.properties.schemaVersion.const,
    "1.2",
  );
  assert.equal(fixtureMutationsSchema.additionalProperties, false);
  assert.equal(
    fixtureMutationsSchema.properties.mutations.items.additionalProperties,
    false,
  );
  assert.deepEqual(
    fixtureValidationSchema.properties.targetProfile.enum,
    [null, ...targetProfileIds],
  );
  assert.deepEqual(
    fixtureValidationSchema.properties.validationProfile.enum,
    validationProfileIds,
  );
  assert.equal(
    fixtureValidationSchema.properties.schemaVersion.const,
    "1.5",
  );
  assert.equal(
    fixtureValidationSchema.properties.validationProfileRevision.minimum,
    1,
  );
  assert.equal(
    fixtureValidationSchema.properties.validationProfileSha256.pattern,
    "^[a-f0-9]{64}$",
  );
  assert.deepEqual(
    fixtureValidationSchema.properties.suite.enum,
    ["firmware", "auxiliary"],
  );
  assert.equal(
    fixtureValidationSchema.allOf[1].then.properties.targetProfile.const,
    null,
  );
  assert.equal(
    fixtureValidationSchema.$defs.toolchain.additionalProperties,
    false,
  );
  assert.equal(
    fixtureValidationSchema.$defs.artifact.properties.sizeBytes.minimum,
    0,
  );
  assert.deepEqual(
    fixtureValidationSchema.$defs.phase.properties.outcome.enum,
    ["error", "failed", "passed", "timed-out"],
  );
  assert.equal(fixtureValidationSchema.additionalProperties, false);
  assert.equal(validationProfilesSchema.properties.schemaVersion.const, "2.2");
  assert.equal(validationProfilesSchema.additionalProperties, false);
  assert.equal(
    validationProfilesSchema.$defs.dependencyInstall.oneOf.length,
    2,
  );
  assert.deepEqual(
    validationProfilesSchema.$defs.testRuntime.required,
    ["mounts", "commandContracts"],
  );
  assert.equal(
    validationProfilesSchema.$defs.testRuntimeMount.properties.access.const,
    "read-only",
  );
  assert.deepEqual(
    validationProfilesSchema.$defs.testRuntimeCommandContract
      .properties.phase.enum,
    ["analyze", "compile", "test"],
  );
  assert.equal(
    validationProfilesSchema.$defs.environment
      .properties.execution.$ref,
    "#/$defs/execution",
  );
  assert.equal(
    validationProfilesSchema.$defs.sandboxPolicyFields
      .properties.network.const,
    "none",
  );
});

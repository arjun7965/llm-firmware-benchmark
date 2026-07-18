import {
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
import {
  exportPublicResults,
  findSensitiveData,
  sanitizeText,
  toPublicResult,
  validatePublicResult,
} from "../src/public-results.mjs";

const uuid = "123e4567-e89b-42d3-a456-426614174000";

function rawResult(stdout) {
  return {
    run: 1,
    task: "example-task",
    category: "testing",
    suite: "firmware",
    scoringMode: "deterministic",
    targetProfile: "portable-c11",
    validationProfile: "c11-host",
    provider: "ncode",
    modelName: "example-model",
    modelId: "/private/models/example-model",
    modelOptions: {
      effort: "medium",
      [["api", "Key"].join("")]: ["must", "not", "be", "exported"].join("-"),
    },
    startedAt: "2026-01-01T00:00:00.000Z",
    finishedAt: "2026-01-01T00:00:01.000Z",
    exitCode: 0,
    signal: null,
    stdout,
    stderr: "private diagnostic",
    error: null,
  };
}

function temporaryDirectory(t) {
  const path = mkdtempSync(join(tmpdir(), "public-results-test-"));
  t.after(() => rmSync(path, { recursive: true, force: true }));
  return path;
}

test("sanitizer removes credential, key, path, and identifier canaries", () => {
  const apiToken = ["sk", "proj", "A".repeat(28)].join("-");
  const privateKey = [
    "-----BEGIN " + "PRIVATE KEY-----",
    "cHJpdmF0ZS1rZXktY2FuYXJ5",
    "-----END " + "PRIVATE KEY-----",
  ].join("\n");
  const homePath = ["/home", "alice", ".ncode", "memory"].join("/");
  const credentialCanary = ["correct", "horse", "canary"].join("-");
  const credentialUrl = ["https://user", "canary@example.test"].join(":");
  const sensitive = [
    apiToken,
    privateKey,
    homePath,
    ["password", " = ", '"', credentialCanary, '"'].join(""),
    credentialUrl,
    uuid,
  ].join("\n");

  const result = sanitizeText(sensitive);

  assert.equal(findSensitiveData(result.text).length, 0);
  for (const canary of [
    apiToken,
    "cHJpdmF0ZS1rZXktY2FuYXJ5",
    "alice",
    credentialCanary,
    "user:canary",
    uuid,
  ]) {
    assert.equal(result.text.includes(canary), false);
  }
  assert.ok(result.redactions.length >= 6);
});

test("public result allowlists fields and extracts NCode answer text", () => {
  const raw = rawResult(JSON.stringify({
    type: "result",
    result: "A legitimate answer.",
    session_id: uuid,
    uuid,
    total_cost_usd: 0.01,
    usage: { input_tokens: 10 },
  }));

  const result = toPublicResult(raw);
  const serialized = JSON.stringify(result);

  assert.equal(result.schemaVersion, "1.4");
  assert.equal(result.answer, "A legitimate answer.");
  assert.equal(result.task.suite, "firmware");
  assert.equal(result.task.scoringMode, "deterministic");
  assert.equal(result.task.targetProfile, "portable-c11");
  assert.equal(result.task.validationProfile, "c11-host");
  assert.equal(result.publication.reviewRequired, false);
  assert.equal(serialized.includes(uuid), false);
  assert.equal(serialized.includes("modelId"), false);
  assert.equal(serialized.includes("modelOptions"), false);
  assert.equal(serialized.includes("private diagnostic"), false);
  assert.equal(serialized.includes("total_cost_usd"), false);
});

test("public result reproduces and blocks the original disclosure path", () => {
  const credential = ["do", "not", "publish"].join("-");
  const homePath = ["/home", "alice", ".ncode", "memory", "MEMORY.md"]
    .join("/");
  const answer = [
    ["passwd", ": ", credential].join(""),
    `read ${homePath}`,
    `session_id=${uuid}`,
  ].join("\n");
  const result = toPublicResult(rawResult(JSON.stringify({ result: answer })));
  const serialized = JSON.stringify(result);

  assert.equal(result.publication.reviewRequired, true);
  assert.equal(serialized.includes(credential), false);
  assert.equal(serialized.includes("alice"), false);
  assert.equal(serialized.includes(uuid), false);
  assert.equal(findSensitiveData(serialized).length, 0);
});

test("directory export writes only validated public projections", (t) => {
  const root = temporaryDirectory(t);
  const input = join(root, "raw");
  const output = join(root, "public");
  const nested = join(input, "run-2");
  mkdirSync(nested, { recursive: true });
  writeFileSync(
    join(nested, "task--model.json"),
    JSON.stringify(rawResult("Safe plain-text answer.")),
  );

  const summary = exportPublicResults({
    inputDir: input,
    outputDir: output,
  });
  const exported = JSON.parse(
    readFileSync(join(output, "run-2", "task--model.json"), "utf8"),
  );

  assert.deepEqual(summary, {
    fileCount: 1,
    redactionCount: 0,
    reviewFileCount: 0,
  });
  assert.equal(exported.answer, "Safe plain-text answer.");
  assert.equal(validatePublicResult(exported), exported);
});

test("provider metadata envelopes without an answer are rejected", () => {
  assert.throws(
    () => toPublicResult(rawResult(JSON.stringify({
      session_id: uuid,
      usage: {},
    }))),
    /does not contain a string result/,
  );
});

test("public result validation rejects extra fields", () => {
  const result = toPublicResult(rawResult("Safe answer."));

  assert.throws(
    () => validatePublicResult({ ...result, schemaVersion: "1.2" }),
    /schemaVersion/,
  );
  assert.throws(
    () => validatePublicResult({ ...result, stdout: "raw" }),
    /unexpected fields/,
  );
  assert.throws(
    () => validatePublicResult({
      ...result,
      execution: {
        ...result.execution,
        exitCode: { diagnostic: "must not be exported" },
      },
    }),
    /exitCode/,
  );
  assert.throws(
    () => validatePublicResult({
      ...result,
      publication: {
        ...result.publication,
        extra: "raw metadata",
      },
    }),
    /unexpected fields/,
  );
  assert.throws(
    () => validatePublicResult({
      ...result,
      task: {
        ...result.task,
        targetProfile: "unknown-profile",
      },
    }),
    /targetProfile/,
  );
  assert.throws(
    () => validatePublicResult({
      ...result,
      task: {
        ...result.task,
        scoringMode: "manual",
      },
    }),
    /scoringMode/,
  );
  assert.throws(
    () => validatePublicResult({
      ...result,
      task: {
        ...result.task,
        validationProfile: "unknown-profile",
      },
    }),
    /validationProfile/,
  );
  assert.throws(
    () => validatePublicResult({
      ...result,
      task: {
        ...result.task,
        suite: "unknown",
      },
    }),
    /task.suite/,
  );
  assert.throws(
    () => validatePublicResult({
      ...result,
      task: {
        ...result.task,
        suite: "auxiliary",
      },
    }),
    /auxiliary task.*targetProfile/,
  );
});

test("legacy results infer suite metadata and export safely", () => {
  const raw = rawResult("Legacy answer.");
  delete raw.provider;
  delete raw.suite;
  delete raw.scoringMode;
  delete raw.targetProfile;

  const result = toPublicResult(raw);

  assert.equal(result.model.provider, "unknown");
  assert.equal(result.task.suite, "auxiliary");
  assert.equal(result.task.scoringMode, "rubric-only");
  assert.equal(result.task.targetProfile, null);
  assert.equal(result.task.validationProfile, "c11-host");
  assert.equal(result.answer, "Legacy answer.");

  assert.throws(
    () => toPublicResult({
      ...raw,
      validationProfile: undefined,
    }),
    /validationProfile/,
  );

  const profileBackedRaw = rawResult("Legacy firmware answer.");
  delete profileBackedRaw.suite;
  const profileBackedResult = toPublicResult(profileBackedRaw);
  assert.equal(profileBackedResult.task.suite, "firmware");

  const legacyPublicResult = {
    ...result,
    schemaVersion: "1.3",
    task: {
      ...result.task,
    },
  };
  delete legacyPublicResult.task.scoringMode;
  assert.equal(
    validatePublicResult(legacyPublicResult),
    legacyPublicResult,
  );
});

import { createHash } from "node:crypto";
import {
  readFileSync,
  readdirSync,
} from "node:fs";
import { fileURLToPath } from "node:url";
import test from "node:test";
import assert from "node:assert/strict";

const schemasRoot = new URL("../schemas/", import.meta.url);
const versionPattern = /^\d+\.\d+$/u;
const fingerprintPattern = /^[a-f0-9]{64}$/u;

function canonicalize(value) {
  if (Array.isArray(value)) {
    return value.map(canonicalize);
  }
  if (value && typeof value === "object") {
    return Object.fromEntries(
      Object.keys(value)
        .sort()
        .map((key) => [key, canonicalize(value[key])]),
    );
  }
  return value;
}

function fingerprint(schema) {
  return createHash("sha256")
    .update(JSON.stringify(canonicalize(schema)))
    .digest("hex");
}

function compareVersions(left, right) {
  const [leftMajor, leftMinor] = left.split(".").map(Number);
  const [rightMajor, rightMinor] = right.split(".").map(Number);
  return leftMajor - rightMajor || leftMinor - rightMinor;
}

test("versioned schema changes require a new contract fingerprint", () => {
  const history = JSON.parse(
    readFileSync(
      new URL("../schemas/contract-fingerprints.json", import.meta.url),
      "utf8",
    ),
  );
  const versionedSchemas = new Map();

  for (const file of readdirSync(fileURLToPath(schemasRoot)).sort()) {
    if (!file.endsWith(".schema.json")) continue;
    const schema = JSON.parse(
      readFileSync(new URL(file, schemasRoot), "utf8"),
    );
    const version = schema.properties?.schemaVersion?.const;
    if (version !== undefined) {
      versionedSchemas.set(file, { schema, version });
    }
  }

  assert.deepEqual(
    Object.keys(history).sort(),
    [...versionedSchemas.keys()].sort(),
    "fingerprint history must cover every versioned schema",
  );

  for (const [file, { schema, version }] of versionedSchemas) {
    assert.match(version, versionPattern, `${file} has an invalid version`);
    const versions = Object.keys(history[file]);
    assert.deepEqual(
      versions,
      [...versions].sort(compareVersions),
      `${file} fingerprint history must be ordered`,
    );
    assert.equal(
      versions.at(-1),
      version,
      `${file} current version must be the newest fingerprint`,
    );
    for (const hash of Object.values(history[file])) {
      assert.match(hash, fingerprintPattern, `${file} has an invalid hash`);
    }
    assert.equal(
      history[file][version],
      fingerprint(schema),
      `${file} changed without a new schema version and fingerprint`,
    );
  }
});

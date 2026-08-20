import { readFileSync } from "node:fs";

import {
  hilCatalogReference,
  hilTestIds,
  validateHilCatalog,
} from "./hil-targets.mjs";

const hashPattern = /^[a-f0-9]{64}$/u;
const revisionPattern = /^[A-Za-z0-9][A-Za-z0-9._ -]*$/u;
const sourceRevisionPattern = /^[a-f0-9]{40}$/u;
const timestampPattern =
  /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d+)?(?:Z|[+-]\d{2}:\d{2})$/u;
const versionPattern = /^[A-Za-z0-9][A-Za-z0-9 ._()+-]*$/u;
const reportFields = [
  "board",
  "catalog",
  "dependencies",
  "finishedAt",
  "firmware",
  "schemaVersion",
  "startedAt",
  "success",
  "targetId",
  "tests",
];
const catalogFields = ["schemaVersion", "sha256"];
const boardFields = [
  "hardwareRevision",
  "probeSerialSha256",
  "wiringRevision",
];
const firmwareFields = ["imageSha256", "sourceRevision"];
const dependencyFields = ["kind", "name", "version"];
const testFields = ["durationMs", "evidenceSha256", "id", "status"];
const dependencyKinds = ["probe-tool", "toolchain", "sdk"];
const testStatuses = new Set(["pass", "fail"]);

function requireObject(value, name) {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    throw new TypeError(`${name} must be an object`);
  }
}

function requireExactFields(value, fields, name) {
  requireObject(value, name);
  if (Object.keys(value).sort().join(",") !== [...fields].sort().join(",")) {
    throw new TypeError(`${name} has unexpected fields`);
  }
}

function requireString(value, name, pattern, maxLength = 1024) {
  if (
    typeof value !== "string" ||
    value.trim() === "" ||
    value.length > maxLength ||
    (pattern && !pattern.test(value))
  ) {
    throw new TypeError(`${name} is invalid`);
  }
}

function requireTimestamp(value, name) {
  requireString(value, name, timestampPattern);
  const timestamp = Date.parse(value);
  if (!Number.isFinite(timestamp)) {
    throw new TypeError(`${name} must be an ISO 8601 timestamp`);
  }
  return timestamp;
}

export function validateHilReport(report, catalog) {
  validateHilCatalog(catalog);
  requireExactFields(report, reportFields, "HIL report");
  if (report.schemaVersion !== "1.0") {
    throw new TypeError("unsupported HIL report schemaVersion");
  }

  requireExactFields(report.catalog, catalogFields, "HIL report catalog");
  const expectedCatalog = hilCatalogReference(catalog);
  if (
    report.catalog.schemaVersion !== expectedCatalog.schemaVersion ||
    report.catalog.sha256 !== expectedCatalog.sha256
  ) {
    throw new TypeError("HIL report catalog reference does not match");
  }

  const target = catalog.targets.find((item) => item.id === report.targetId);
  if (!target) {
    throw new TypeError(`HIL report target is unknown: ${report.targetId}`);
  }
  const startedAt = requireTimestamp(report.startedAt, "HIL report startedAt");
  const finishedAt = requireTimestamp(
    report.finishedAt,
    "HIL report finishedAt",
  );
  if (finishedAt < startedAt) {
    throw new TypeError("HIL report finishedAt precedes startedAt");
  }

  requireExactFields(report.board, boardFields, "HIL report board");
  requireString(
    report.board.hardwareRevision,
    "HIL report board.hardwareRevision",
    revisionPattern,
    128,
  );
  requireString(
    report.board.wiringRevision,
    "HIL report board.wiringRevision",
    revisionPattern,
    128,
  );
  requireString(
    report.board.probeSerialSha256,
    "HIL report board.probeSerialSha256",
    hashPattern,
    64,
  );

  requireExactFields(report.firmware, firmwareFields, "HIL report firmware");
  requireString(
    report.firmware.sourceRevision,
    "HIL report firmware.sourceRevision",
    sourceRevisionPattern,
    40,
  );
  requireString(
    report.firmware.imageSha256,
    "HIL report firmware.imageSha256",
    hashPattern,
    64,
  );

  if (!Array.isArray(report.dependencies) || report.dependencies.length !== 3) {
    throw new TypeError("HIL report dependencies must contain three entries");
  }
  const expectedDependencyNames = [
    target.probe.programmer,
    target.toolchain.name,
    target.sdk.name,
  ];
  for (const [index, dependency] of report.dependencies.entries()) {
    const name = `HIL report dependency ${index}`;
    requireExactFields(dependency, dependencyFields, name);
    if (
      dependency.kind !== dependencyKinds[index] ||
      dependency.name !== expectedDependencyNames[index]
    ) {
      throw new TypeError(`${name} does not match the target catalog`);
    }
    requireString(
      dependency.version,
      `${name}.version`,
      versionPattern,
      256,
    );
  }

  if (!Array.isArray(report.tests) || report.tests.length !== hilTestIds.length) {
    throw new TypeError("HIL report tests must cover the common protocol");
  }
  for (const [index, testResult] of report.tests.entries()) {
    const name = `HIL report test ${index}`;
    requireExactFields(testResult, testFields, name);
    if (testResult.id !== hilTestIds[index]) {
      throw new TypeError(`${name} does not match the common protocol`);
    }
    if (!Number.isInteger(testResult.durationMs) || testResult.durationMs < 0) {
      throw new TypeError(`${name}.durationMs is invalid`);
    }
    if (!testStatuses.has(testResult.status)) {
      throw new TypeError(`${name}.status is invalid`);
    }
    requireString(
      testResult.evidenceSha256,
      `${name}.evidenceSha256`,
      hashPattern,
      64,
    );
  }
  if (typeof report.success !== "boolean") {
    throw new TypeError("HIL report success must be boolean");
  }
  const expectedSuccess = report.tests.every((testResult) =>
    testResult.status === "pass");
  if (report.success !== expectedSuccess) {
    throw new TypeError("HIL report success does not match test outcomes");
  }
  return report;
}

export function loadHilReport(file, catalog) {
  if (!file) throw new TypeError("HIL report path is required");
  return validateHilReport(JSON.parse(readFileSync(file, "utf8")), catalog);
}

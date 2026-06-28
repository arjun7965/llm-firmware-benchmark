import { createHash } from "node:crypto";
import {
  mkdirSync,
  readFileSync,
  readdirSync,
  renameSync,
  statSync,
  writeFileSync,
} from "node:fs";
import { dirname, join, relative, resolve } from "node:path";

const sensitiveRules = [
  {
    type: "private-key",
    kind: "secret",
    pattern: /-----BEGIN ((?:RSA |EC |OPENSSH |DSA |PGP )?PRIVATE KEY)-----[\s\S]*?-----END \1-----/g,
    replace: () => "[REDACTED_PRIVATE_KEY]",
  },
  {
    type: "api-token",
    kind: "secret",
    pattern: /\b(?:sk-(?:proj-|svcacct-)?[A-Za-z0-9_-]{20,}|sk-ant-[A-Za-z0-9_-]{20,}|gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,}|(?:AKIA|ASIA)[A-Z0-9]{16}|AIza[0-9A-Za-z_-]{30,}|xox[baprs]-[A-Za-z0-9-]{10,}|(?:sk|rk)_live_[A-Za-z0-9]{16,}|npm_[A-Za-z0-9]{20,}|hf_[A-Za-z0-9]{20,}|glpat-[A-Za-z0-9_-]{20,}|pypi-[A-Za-z0-9_-]{20,})\b/g,
    replace: () => "[REDACTED_API_TOKEN]",
  },
  {
    type: "jwt",
    kind: "secret",
    pattern: /\beyJ[A-Za-z0-9_-]{10,}\.[A-Za-z0-9_-]{10,}\.[A-Za-z0-9_-]{10,}\b/g,
    replace: () => "[REDACTED_JWT]",
  },
  {
    type: "credential-url",
    kind: "secret",
    pattern: /\b([a-z][a-z0-9+.-]*:\/\/)[^\s/:]+:[^\s/@]+@/gi,
    replace: (_match, prefix) => `${prefix}[REDACTED]@`,
  },
  {
    type: "credential-assignment",
    kind: "secret",
    pattern: /\b(api[_-]?key|access[_-]?token|auth[_-]?token|client[_-]?secret|password|passwd|secret)(\s*[:=]\s*)(?!["']?\[REDACTED)(?:"[^"\n]*"|'[^'\n]*'|[^\s,;}\]]+)/gi,
    replace: (_match, key, separator) => `${key}${separator}[REDACTED]`,
  },
  {
    type: "home-path",
    kind: "privacy",
    pattern: /\/(?:home|Users)\/[^/\\\s"'`]+/g,
    replace: () => "$HOME",
  },
  {
    type: "windows-home-path",
    kind: "privacy",
    pattern: /\b[A-Za-z]:\\Users\\[^\\\s"'`]+/g,
    replace: () => "%USERPROFILE%",
  },
  {
    type: "session-identifier",
    kind: "metadata",
    pattern: /(["'](?:session[_-]?id|sessionId|uuid)["']\s*[:=]\s*["'])(?!\[REDACTED)[^"'\n]+(["'])/gi,
    replace: (_match, prefix, suffix) =>
      `${prefix}[REDACTED_SESSION_ID]${suffix}`,
  },
  {
    type: "session-identifier",
    kind: "metadata",
    pattern: /\b(session[_-]?id|sessionId|uuid)(\s*[:=]\s*)(?!\[REDACTED)[A-Za-z0-9._-]{8,}/gi,
    replace: (_match, key, separator) =>
      `${key}${separator}[REDACTED_SESSION_ID]`,
  },
  {
    type: "uuid",
    kind: "metadata",
    pattern: /\b[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}\b/gi,
    replace: () => "[REDACTED_UUID]",
  },
];

function ruleEnabled(rule, {
  includePrivacy = true,
  includeMetadata = true,
} = {}) {
  if (rule.kind === "privacy" && !includePrivacy) return false;
  if (rule.kind === "metadata" && !includeMetadata) return false;
  return true;
}

export function findSensitiveData(text, options = {}) {
  if (typeof text !== "string") {
    throw new TypeError("text must be a string");
  }

  const findings = [];
  for (const rule of sensitiveRules) {
    if (!ruleEnabled(rule, options)) continue;
    rule.pattern.lastIndex = 0;
    let match;
    while ((match = rule.pattern.exec(text))) {
      findings.push({
        type: rule.type,
        line: text.slice(0, match.index).split("\n").length,
      });
      if (match[0].length === 0) rule.pattern.lastIndex++;
    }
  }
  return findings;
}

export function sanitizeText(text) {
  if (typeof text !== "string") {
    throw new TypeError("text must be a string");
  }

  let sanitized = text;
  const counts = new Map();
  for (const rule of sensitiveRules) {
    rule.pattern.lastIndex = 0;
    sanitized = sanitized.replace(rule.pattern, (...args) => {
      counts.set(rule.type, (counts.get(rule.type) ?? 0) + 1);
      return rule.replace(...args);
    });
  }

  return {
    text: sanitized,
    redactions: [...counts.entries()]
      .map(([type, count]) => ({ type, count }))
      .sort((a, b) => a.type.localeCompare(b.type)),
  };
}

export function extractAnswer(stdout) {
  if (typeof stdout !== "string") {
    throw new TypeError("result stdout must be a string");
  }

  try {
    const envelope = JSON.parse(stdout);
    if (envelope && typeof envelope === "object" && !Array.isArray(envelope)) {
      if (typeof envelope.result === "string") return envelope.result;
      const metadataKeys = [
        "session_id",
        "uuid",
        "usage",
        "modelUsage",
        "total_cost_usd",
      ];
      if (metadataKeys.some((key) => key in envelope)) {
        throw new TypeError(
          "provider metadata envelope does not contain a string result",
        );
      }
    }
  } catch (error) {
    if (error instanceof SyntaxError) return stdout;
    throw error;
  }

  return stdout;
}

function requireString(value, name) {
  if (typeof value !== "string" || value.trim() === "") {
    throw new TypeError(`${name} must be a non-empty string`);
  }
  return value;
}

function requireExactKeys(value, keys, name) {
  if (!value || typeof value !== "object" || Array.isArray(value) ||
      Object.keys(value).sort().join(",") !== [...keys].sort().join(",")) {
    throw new TypeError(`${name} has unexpected fields`);
  }
}

function requireDateTime(value, name) {
  requireString(value, name);
  if (!/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d+)?(?:Z|[+-]\d{2}:\d{2})$/
    .test(value) || Number.isNaN(Date.parse(value))) {
    throw new TypeError(`${name} must be an ISO 8601 date-time`);
  }
}

export function toPublicResult(rawResult, source = JSON.stringify(rawResult)) {
  if (!rawResult || typeof rawResult !== "object" ||
      Array.isArray(rawResult)) {
    throw new TypeError("raw result must be an object");
  }

  const answer = sanitizeText(extractAnswer(rawResult.stdout));
  const publicResult = {
    schemaVersion: "1.0",
    task: {
      id: requireString(rawResult.task, "task"),
      category: requireString(rawResult.category, "category"),
    },
    model: {
      id: requireString(rawResult.modelName, "modelName"),
      provider: requireString(rawResult.provider ?? "unknown", "provider"),
    },
    run: rawResult.run ?? 1,
    execution: {
      startedAt: requireString(rawResult.startedAt, "startedAt"),
      finishedAt: requireString(rawResult.finishedAt, "finishedAt"),
      exitCode: rawResult.exitCode ?? null,
      signal: rawResult.signal ?? null,
    },
    answer: answer.text,
    publication: {
      sourceSha256: createHash("sha256").update(source).digest("hex"),
      reviewRequired: answer.redactions.length > 0,
      redactions: answer.redactions,
    },
  };

  validatePublicResult(publicResult);
  return publicResult;
}

export function validatePublicResult(result) {
  const topLevelKeys = [
    "answer",
    "execution",
    "model",
    "publication",
    "run",
    "schemaVersion",
    "task",
  ];
  requireExactKeys(result, topLevelKeys, "public result");
  if (result.schemaVersion !== "1.0") {
    throw new TypeError("unsupported public result schemaVersion");
  }
  requireExactKeys(result.task, ["category", "id"], "task");
  requireExactKeys(result.model, ["id", "provider"], "model");
  requireExactKeys(
    result.execution,
    ["exitCode", "finishedAt", "signal", "startedAt"],
    "execution",
  );
  requireExactKeys(
    result.publication,
    ["redactions", "reviewRequired", "sourceSha256"],
    "publication",
  );
  requireString(result.task?.id, "task.id");
  requireString(result.task?.category, "task.category");
  requireString(result.model?.id, "model.id");
  requireString(result.model?.provider, "model.provider");
  if (!Number.isInteger(result.run) || result.run < 1) {
    throw new TypeError("run must be a positive integer");
  }
  requireDateTime(result.execution.startedAt, "execution.startedAt");
  requireDateTime(result.execution.finishedAt, "execution.finishedAt");
  if (result.execution.exitCode !== null &&
      !Number.isInteger(result.execution.exitCode)) {
    throw new TypeError("execution.exitCode must be an integer or null");
  }
  if (result.execution.signal !== null &&
      typeof result.execution.signal !== "string") {
    throw new TypeError("execution.signal must be a string or null");
  }
  if (typeof result.answer !== "string") {
    throw new TypeError("answer must be a string");
  }
  if (typeof result.publication?.sourceSha256 !== "string" ||
      !/^[a-f0-9]{64}$/.test(result.publication.sourceSha256)) {
    throw new TypeError("publication.sourceSha256 must be SHA-256");
  }
  if (typeof result.publication.reviewRequired !== "boolean" ||
      !Array.isArray(result.publication.redactions)) {
    throw new TypeError("publication review metadata is invalid");
  }
  const redactionTypes = new Set();
  for (const redaction of result.publication.redactions) {
    requireExactKeys(redaction, ["count", "type"], "redaction");
    requireString(redaction.type, "redaction.type");
    if (!Number.isInteger(redaction.count) || redaction.count < 1) {
      throw new TypeError("redaction.count must be a positive integer");
    }
    if (redactionTypes.has(redaction.type)) {
      throw new TypeError("redaction types must be unique");
    }
    redactionTypes.add(redaction.type);
  }
  if (result.publication.reviewRequired !==
      (result.publication.redactions.length > 0)) {
    throw new TypeError("publication.reviewRequired is inconsistent");
  }

  const remaining = findSensitiveData(JSON.stringify(result));
  if (remaining.length > 0) {
    throw new TypeError(
      `public result still contains sensitive data: ${remaining[0].type}`,
    );
  }
  return result;
}

function walkJsonFiles(root, directory = root, files = []) {
  for (const entry of readdirSync(directory, { withFileTypes: true })) {
    const path = join(directory, entry.name);
    if (entry.isSymbolicLink()) {
      throw new TypeError(`symbolic links are not allowed: ${path}`);
    }
    if (entry.isDirectory()) {
      walkJsonFiles(root, path, files);
    } else if (entry.isFile() && entry.name.endsWith(".json")) {
      files.push(path);
    }
  }
  return files;
}

export function exportPublicResults({ inputDir, outputDir }) {
  const inputRoot = resolve(inputDir);
  const outputRoot = resolve(outputDir);
  const relativeOutput = relative(inputRoot, outputRoot);
  if (relativeOutput === "" ||
      (!relativeOutput.startsWith("..") && relativeOutput !== "..")) {
    throw new TypeError("output directory must be outside the input directory");
  }
  if (!statSync(inputRoot).isDirectory()) {
    throw new TypeError("input must be a directory");
  }

  const files = walkJsonFiles(inputRoot).sort();
  if (files.length === 0) {
    throw new TypeError("input directory contains no JSON result files");
  }

  let redactionCount = 0;
  let reviewFileCount = 0;
  for (const inputPath of files) {
    const source = readFileSync(inputPath);
    const rawResult = JSON.parse(source.toString("utf8"));
    const publicResult = toPublicResult(rawResult, source);
    const relativePath = relative(inputRoot, inputPath);
    const outputPath = join(outputRoot, relativePath);
    const temporaryPath = `${outputPath}.tmp`;
    mkdirSync(dirname(outputPath), { recursive: true });
    writeFileSync(temporaryPath, JSON.stringify(publicResult, null, 2));
    renameSync(temporaryPath, outputPath);

    const fileRedactions = publicResult.publication.redactions
      .reduce((total, item) => total + item.count, 0);
    redactionCount += fileRedactions;
    if (fileRedactions > 0) reviewFileCount++;
  }

  return {
    fileCount: files.length,
    redactionCount,
    reviewFileCount,
  };
}

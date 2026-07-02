import { randomUUID } from "node:crypto";
import {
  existsSync,
  lstatSync,
  mkdirSync,
  readFileSync,
  renameSync,
  rmSync,
  statSync,
  writeFileSync,
} from "node:fs";
import {
  basename,
  dirname,
  join,
  relative,
  resolve,
  sep,
} from "node:path";
import { fileURLToPath } from "node:url";
import { extractAnswer } from "./answers.mjs";
import { validateFixtureManifest } from "./fixtures.mjs";
import {
  loadTasks,
  promptSha256,
} from "./harness.mjs";

const maximumResultBytes = 5 * 1024 * 1024;
const maximumAnswerBytes = 1024 * 1024;
const maximumCodeBytes = 256 * 1024;
const taskIdPattern = /^[a-z0-9]+(?:-[a-z0-9]+)*$/;

function asPath(value) {
  return value instanceof URL ? fileURLToPath(value) : value;
}

function byteLength(value) {
  return Buffer.byteLength(value, "utf8");
}

function requirePositiveLimit(value, name) {
  if (!Number.isSafeInteger(value) || value < 1) {
    throw new TypeError(`${name} must be a positive safe integer`);
  }
}

function openingFence(line) {
  const match = line.match(/^ {0,3}(`{3,}|~{3,})(.*)$/u);
  if (!match) return null;
  const info = match[2].trim();
  const parts = info === "" ? [] : info.split(/\s+/u);
  return {
    marker: match[1],
    language: parts[0]?.toLowerCase() ?? "",
    hasExtraInfo: parts.length > 1,
  };
}

function isClosingFence(line, marker) {
  const character = marker[0];
  const match = line.match(/^ {0,3}(`+|~+)[ \t]*$/u);
  return Boolean(
    match &&
    match[1][0] === character &&
    match[1].length >= marker.length,
  );
}

export function extractFencedCode(answer, {
  language,
  answerLimit = maximumAnswerBytes,
  codeLimit = maximumCodeBytes,
} = {}) {
  if (typeof answer !== "string") {
    throw new TypeError("answer must be a string");
  }
  if (typeof language !== "string" || language.trim() === "") {
    throw new TypeError("expected code language must be a non-empty string");
  }
  requirePositiveLimit(answerLimit, "answer limit");
  requirePositiveLimit(codeLimit, "code limit");
  if (answer.includes("\0")) {
    throw new TypeError("answer cannot contain NUL bytes");
  }
  if (byteLength(answer) > answerLimit) {
    throw new RangeError(`answer exceeds ${answerLimit} bytes`);
  }

  const expectedLanguage = language.toLowerCase();
  const lines = answer.replace(/\r\n?/gu, "\n").split("\n");
  const matches = [];

  for (let index = 0; index < lines.length; index++) {
    const opening = openingFence(lines[index]);
    if (!opening) continue;

    let closingIndex = -1;
    for (let candidate = index + 1; candidate < lines.length; candidate++) {
      if (isClosingFence(lines[candidate], opening.marker)) {
        closingIndex = candidate;
        break;
      }
    }
    if (closingIndex === -1) {
      throw new TypeError("answer contains an unterminated fenced code block");
    }

    if (opening.language === expectedLanguage) {
      if (opening.hasExtraInfo) {
        throw new TypeError(
          `expected ${language} fence must contain only its language label`,
        );
      }
      const code = lines.slice(index + 1, closingIndex).join("\n");
      if (code.trim() === "") {
        throw new TypeError(`answer contains an empty ${language} code block`);
      }
      if (byteLength(code) > codeLimit) {
        throw new RangeError(`${language} code exceeds ${codeLimit} bytes`);
      }
      matches.push(code.endsWith("\n") ? code : `${code}\n`);
    }
    index = closingIndex;
  }

  if (matches.length === 0) {
    throw new TypeError(
      `answer must contain exactly one fenced ${language} code block`,
    );
  }
  if (matches.length > 1) {
    throw new TypeError(
      `answer contains multiple fenced ${language} code blocks`,
    );
  }
  return matches[0];
}

function requireContained(root, path, name) {
  const relativePath = relative(root, path);
  if (
    relativePath === "" ||
    relativePath === ".." ||
    relativePath.startsWith(`..${sep}`)
  ) {
    throw new TypeError(`${name} escapes its expected directory`);
  }
}

function requireDirectory(path, name) {
  if (!existsSync(path)) {
    throw new TypeError(`${name} does not exist`);
  }
  const metadata = lstatSync(path);
  if (metadata.isSymbolicLink() || !metadata.isDirectory()) {
    throw new TypeError(`${name} must be a non-symlink directory`);
  }
}

function ensureSafeDirectories(root, relativeDirectory) {
  let current = root;
  for (const segment of relativeDirectory.split("/")) {
    current = join(current, segment);
    if (!existsSync(current)) {
      mkdirSync(current);
      continue;
    }
    const metadata = lstatSync(current);
    if (metadata.isSymbolicLink() || !metadata.isDirectory()) {
      throw new TypeError(
        `fixture output directory is unsafe: ${relative(root, current)}`,
      );
    }
  }
}

function writeCode(outputPath, code, overwrite) {
  if (existsSync(outputPath)) {
    const metadata = lstatSync(outputPath);
    if (metadata.isSymbolicLink() || !metadata.isFile()) {
      throw new TypeError("fixture answer output must be a regular file");
    }
    if (!overwrite) {
      throw new TypeError(
        "fixture answer output already exists; pass --force to replace it",
      );
    }
  }

  if (!overwrite) {
    writeFileSync(outputPath, code, {
      encoding: "utf8",
      flag: "wx",
      mode: 0o600,
    });
    return;
  }

  const temporaryPath = join(
    dirname(outputPath),
    `.${basename(outputPath)}.${randomUUID()}.tmp`,
  );
  try {
    writeFileSync(temporaryPath, code, {
      encoding: "utf8",
      flag: "wx",
      mode: 0o600,
    });
    renameSync(temporaryPath, outputPath);
  } finally {
    rmSync(temporaryPath, { force: true });
  }
}

function readResult(resultPath) {
  const metadata = lstatSync(resultPath);
  if (metadata.isSymbolicLink() || !metadata.isFile()) {
    throw new TypeError("result path must be a non-symlink regular file");
  }
  if (statSync(resultPath).size > maximumResultBytes) {
    throw new RangeError(
      `result file exceeds ${maximumResultBytes} bytes`,
    );
  }
  let result;
  try {
    result = JSON.parse(readFileSync(resultPath, "utf8"));
  } catch (error) {
    throw new TypeError(`result file is not valid JSON: ${error.message}`);
  }
  if (!result || typeof result !== "object" || Array.isArray(result)) {
    throw new TypeError("result must be an object");
  }
  return result;
}

export function extractFixtureAnswer({
  resultPath,
  fixturesRoot,
  tasksPath,
  overwrite = false,
}) {
  if (typeof overwrite !== "boolean") {
    throw new TypeError("overwrite must be a boolean");
  }
  const resolvedResultPath = resolve(asPath(resultPath));
  const resolvedFixturesRoot = resolve(asPath(fixturesRoot));
  const result = readResult(resolvedResultPath);

  if (
    result.exitCode !== 0 ||
    result.signal !== null ||
    result.error !== null
  ) {
    throw new TypeError("only successful provider results can be extracted");
  }
  if (typeof result.task !== "string" || !taskIdPattern.test(result.task)) {
    throw new TypeError("result task is invalid");
  }

  requireDirectory(resolvedFixturesRoot, "fixtures root");
  const tasks = loadTasks(tasksPath);
  const task = tasks.find((item) => item.id === result.task);
  if (!task) throw new TypeError(`result task is unknown: ${result.task}`);
  if (
    result.category !== task.category ||
    result.targetProfile !== (task.targetProfile ?? null)
  ) {
    throw new TypeError("result task metadata does not match tasks.json");
  }
  if (result.promptSha256 !== promptSha256(task.prompt)) {
    throw new TypeError(
      "result prompt hash does not match the current task prompt",
    );
  }

  const fixtureRoot = resolve(resolvedFixturesRoot, result.task);
  requireContained(resolvedFixturesRoot, fixtureRoot, "fixture path");
  requireDirectory(fixtureRoot, `fixture ${result.task}`);
  const manifestPath = join(fixtureRoot, "manifest.json");
  if (!existsSync(manifestPath) || lstatSync(manifestPath).isSymbolicLink()) {
    throw new TypeError(`fixture ${result.task} is missing manifest.json`);
  }
  const manifest = validateFixtureManifest(
    JSON.parse(readFileSync(manifestPath, "utf8")),
    task,
  );

  const answer = extractAnswer(result.stdout);
  const code = extractFencedCode(answer, {
    language: manifest.answer.language,
  });
  const outputPath = resolve(fixtureRoot, manifest.answer.output);
  requireContained(fixtureRoot, outputPath, "fixture answer output");
  ensureSafeDirectories(fixtureRoot, dirname(manifest.answer.output));
  writeCode(outputPath, code, overwrite);

  return {
    taskId: result.task,
    language: manifest.answer.language,
    outputPath,
    byteLength: byteLength(code),
  };
}

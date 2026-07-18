import {
  createHash,
  randomUUID,
} from "node:crypto";
import {
  existsSync,
  lstatSync,
  mkdirSync,
  readFileSync,
  readdirSync,
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
import { summarizeAnswerFiles } from "./fixture-answer-digests.mjs";
import {
  fixtureAnswerFiles,
  validateFixtureManifest,
} from "./fixtures.mjs";
import {
  loadTasks,
  promptSha256,
} from "./harness.mjs";
import { resultSuite } from "./suites.mjs";

const maximumResultBytes = 5 * 1024 * 1024;
const maximumAnswerBytes = 1024 * 1024;
const maximumCodeBytes = 256 * 1024;
const maximumBundleBytes = 512 * 1024;
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
  let fenceCount = 0;

  for (let index = 0; index < lines.length; index++) {
    const opening = openingFence(lines[index]);
    if (!opening) continue;
    fenceCount += 1;

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
  if (fenceCount > 1) {
    throw new TypeError("answer contains additional fenced code blocks");
  }
  return matches[0];
}

function requireSafeBundlePath(value, name) {
  if (
    typeof value !== "string" ||
    value === "" ||
    value.includes("\\") ||
    value.startsWith("/") ||
    /^[A-Za-z]:\//u.test(value) ||
    value.split("/").some((segment) =>
      segment === "" || segment === "." || segment === "..")
  ) {
    throw new TypeError(`${name} must be a safe relative path`);
  }
}

export function extractFileBundle(answer, {
  files,
  answerLimit = maximumAnswerBytes,
  fileLimit = maximumCodeBytes,
  bundleLimit = maximumBundleBytes,
} = {}) {
  if (typeof answer !== "string") {
    throw new TypeError("answer must be a string");
  }
  if (!Array.isArray(files) || files.length < 2) {
    throw new TypeError("expected bundle files must contain at least two files");
  }
  requirePositiveLimit(answerLimit, "answer limit");
  requirePositiveLimit(fileLimit, "file limit");
  requirePositiveLimit(bundleLimit, "bundle limit");
  if (answer.includes("\0")) {
    throw new TypeError("answer cannot contain NUL bytes");
  }
  if (byteLength(answer) > answerLimit) {
    throw new RangeError(`answer exceeds ${answerLimit} bytes`);
  }

  const expected = new Map();
  for (const file of files) {
    if (
      !file ||
      typeof file.language !== "string" ||
      file.language.trim() === ""
    ) {
      throw new TypeError("expected bundle file is invalid");
    }
    requireSafeBundlePath(file.path, "expected bundle file path");
    if (expected.has(file.path)) {
      throw new TypeError("expected bundle file paths must be unique");
    }
    expected.set(file.path, file.language.toLowerCase());
  }

  const lines = answer.replace(/\r\n?/gu, "\n").split("\n");
  const matches = [];
  const seen = new Set();
  let aggregateBytes = 0;

  for (let index = 0; index < lines.length; index++) {
    const heading = lines[index].match(/^### `([^`]+)`$/u);
    const unheadedFence = openingFence(lines[index]);
    if (!heading) {
      if (unheadedFence) {
        throw new TypeError(
          "every bundle code block must immediately follow a file heading",
        );
      }
      continue;
    }

    const path = heading[1];
    requireSafeBundlePath(path, "answer bundle file path");
    if (!expected.has(path)) {
      throw new TypeError(`answer bundle contains undeclared file: ${path}`);
    }
    if (seen.has(path)) {
      throw new TypeError(`answer bundle contains duplicate file: ${path}`);
    }
    const expectedFile = files[matches.length];
    if (!expectedFile || path !== expectedFile.path) {
      throw new TypeError("answer bundle files are not in manifest order");
    }

    const opening = openingFence(lines[index + 1] ?? "");
    if (!opening) {
      throw new TypeError(
        `answer bundle heading for ${path} must be followed by a code block`,
      );
    }
    const language = expected.get(path);
    if (opening.language !== language || opening.hasExtraInfo) {
      throw new TypeError(
        `answer bundle file ${path} must use only the ${language} language label`,
      );
    }

    let closingIndex = -1;
    for (let candidate = index + 2; candidate < lines.length; candidate++) {
      if (isClosingFence(lines[candidate], opening.marker)) {
        closingIndex = candidate;
        break;
      }
    }
    if (closingIndex === -1) {
      throw new TypeError(
        `answer bundle file ${path} has an unterminated code block`,
      );
    }
    const unnormalizedCode = lines.slice(index + 2, closingIndex).join("\n");
    if (unnormalizedCode.trim() === "") {
      throw new TypeError(`answer bundle file ${path} is empty`);
    }
    const code = unnormalizedCode.endsWith("\n")
      ? unnormalizedCode
      : `${unnormalizedCode}\n`;
    const codeBytes = byteLength(code);
    if (codeBytes > fileLimit) {
      throw new RangeError(
        `answer bundle file ${path} exceeds ${fileLimit} bytes`,
      );
    }
    aggregateBytes += codeBytes;
    if (aggregateBytes > bundleLimit) {
      throw new RangeError(`answer bundle exceeds ${bundleLimit} bytes`);
    }
    matches.push({
      content: code,
      language,
      path,
    });
    seen.add(path);
    index = closingIndex;
  }

  if (matches.length !== files.length) {
    const missing = files
      .filter((file) => !seen.has(file.path))
      .map((file) => file.path)
      .join(", ");
    throw new TypeError(`answer bundle is missing declared files: ${missing}`);
  }
  return matches;
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
  if (relativeDirectory === "." || relativeDirectory === "") return;
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

export function writeFileBundle({
  fixtureRoot,
  generatedDirectory,
  files,
  overwrite = false,
  renameImpl = renameSync,
}) {
  if (typeof overwrite !== "boolean") {
    throw new TypeError("overwrite must be a boolean");
  }
  if (!Array.isArray(files) || files.length < 2) {
    throw new TypeError("answer bundle files must contain at least two files");
  }
  requireSafeBundlePath(generatedDirectory, "generated directory");
  const root = resolve(asPath(fixtureRoot));
  const generatedRoot = resolve(root, generatedDirectory);
  requireContained(root, generatedRoot, "fixture answer bundle output");
  if (existsSync(generatedRoot)) {
    const metadata = lstatSync(generatedRoot);
    if (metadata.isSymbolicLink() || !metadata.isDirectory()) {
      throw new TypeError(
        "fixture answer bundle output must be a non-symlink directory",
      );
    }
    if (!overwrite && readdirSync(generatedRoot).length > 0) {
      throw new TypeError(
        "fixture answer bundle output already exists; pass --force to replace it",
      );
    }
  }

  const transactionId = randomUUID();
  const stagingRoot = join(root, `.${basename(generatedRoot)}.${transactionId}.stage`);
  const backupRoot = join(root, `.${basename(generatedRoot)}.${transactionId}.backup`);
  mkdirSync(stagingRoot, { mode: 0o700 });
  let movedExisting = false;
  let installed = false;
  try {
    for (const file of files) {
      requireSafeBundlePath(file.path, "answer bundle file path");
      if (typeof file.content !== "string" || file.content.includes("\0")) {
        throw new TypeError(`answer bundle file ${file.path} is invalid`);
      }
      ensureSafeDirectories(stagingRoot, dirname(file.path));
      writeFileSync(join(stagingRoot, file.path), file.content, {
        encoding: "utf8",
        flag: "wx",
        mode: 0o600,
      });
    }
    if (existsSync(generatedRoot)) {
      renameImpl(generatedRoot, backupRoot);
      movedExisting = true;
    }
    renameImpl(stagingRoot, generatedRoot);
    installed = true;
    if (movedExisting) {
      rmSync(backupRoot, { recursive: true, force: true });
      movedExisting = false;
    }
  } catch (error) {
    if (installed) {
      rmSync(generatedRoot, { recursive: true, force: true });
      installed = false;
    }
    if (movedExisting && !existsSync(generatedRoot)) {
      try {
        renameImpl(backupRoot, generatedRoot);
        movedExisting = false;
      } catch (rollbackError) {
        throw new AggregateError(
          [error, rollbackError],
          "fixture answer bundle write and rollback failed",
        );
      }
    }
    throw error;
  } finally {
    if (!installed) {
      rmSync(stagingRoot, { recursive: true, force: true });
    }
    if (!movedExisting) {
      rmSync(backupRoot, { recursive: true, force: true });
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
  if (task.scoringMode !== "deterministic") {
    throw new TypeError(
      `rubric-only task ${task.id} cannot use fixture answer extraction`,
    );
  }
  if (
    result.category !== task.category ||
    resultSuite(result) !== task.suite ||
    result.targetProfile !== (task.targetProfile ?? null) ||
    result.scoringMode !== task.scoringMode ||
    result.validationProfile !== task.validationProfile
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
  if (manifest.answer.format === "markdown-fenced-code") {
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
      sha256: createHash("sha256").update(code).digest("hex"),
    };
  }

  const bundle = extractFileBundle(answer, {
    files: manifest.answer.files,
  });
  writeFileBundle({
    fixtureRoot,
    generatedDirectory: manifest.paths.generated,
    files: bundle,
    overwrite,
  });
  const outputFiles = fixtureAnswerFiles(manifest).map((file, index) => ({
    content: bundle[index].content,
    path: file.path,
  }));
  const summary = summarizeAnswerFiles(outputFiles, { bundle: true });
  return {
    taskId: result.task,
    format: manifest.answer.format,
    files: summary.files.map((file) => ({
      ...file,
      outputPath: resolve(fixtureRoot, file.path),
    })),
    byteLength: summary.byteLength,
    sha256: summary.sha256,
  };
}

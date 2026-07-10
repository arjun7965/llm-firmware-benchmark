import {
  existsSync,
  lstatSync,
  readFileSync,
  readdirSync,
} from "node:fs";
import { join, relative, resolve, sep } from "node:path";
import { fileURLToPath } from "node:url";
import { loadTasks } from "./harness.mjs";
import {
  getValidationEnvironmentRevision,
  getValidationProfile,
  requireValidationProfile,
  sandboxProfileBlockReason,
  validationProfileCommandContract,
} from "./validation-profiles.mjs";

const taskIdPattern = /^[a-z0-9]+(?:-[a-z0-9]+)*$/;
const identifierPattern = /^[a-z][a-z0-9-]*$/;
const languagePattern = /^[a-z][a-z0-9+._-]*$/;
const toolPattern = /^[a-z0-9][a-z0-9+._-]*$/;
const fixtureStatuses = new Set(["active", "scaffold"]);
const commandPhases = new Set(["analyze", "compile", "test"]);
const forbiddenCommandTools = new Set([
  "bash",
  "cmd",
  "powershell",
  "pwsh",
  "sh",
  "zsh",
]);
const manifestFields = [
  "answer",
  "commands",
  "language",
  "paths",
  "schemaVersion",
  "status",
  "targetProfile",
  "taskId",
  "toolVersionArgs",
  "validationProfile",
];
const answerFields = ["format", "language", "output"];
const pathFields = [
  "build",
  "generated",
  "mocks",
  "publicTests",
  "reference",
  "scripts",
  "starter",
];
const commandFields = [
  "argv",
  "id",
  "phase",
  "requiredTools",
  "timeoutMs",
];
const trackedDirectoryFields = [
  "mocks",
  "publicTests",
  "reference",
  "scripts",
  "starter",
];
const requiredPaths = {
  build: "build",
  generated: "generated",
  mocks: "mocks",
  publicTests: "tests/public",
  reference: "reference",
  scripts: "scripts",
  starter: "starter",
};

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

function requireString(value, name, pattern) {
  if (typeof value !== "string" || value.trim() === "" ||
      (pattern && !pattern.test(value))) {
    throw new TypeError(`${name} is invalid`);
  }
}

function requireSafeRelativePath(value, name) {
  requireString(value, name);
  if (value.includes("\\") || value.startsWith("/") ||
      value.split("/").some((segment) =>
        segment === "" || segment === "." || segment === "..")) {
    throw new TypeError(`${name} must be a safe relative path`);
  }
}

export function validateFixtureManifest(manifest, task) {
  requireExactFields(manifest, manifestFields, "fixture manifest");
  requireObject(task, "fixture task");
  if (manifest.schemaVersion !== "1.3") {
    throw new TypeError("unsupported fixture schemaVersion");
  }
  requireString(manifest.taskId, "fixture taskId", taskIdPattern);
  if (manifest.taskId !== task.id) {
    throw new TypeError(
      `fixture taskId ${manifest.taskId} does not match ${task.id}`,
    );
  }
  if (manifest.targetProfile !== (task.targetProfile ?? null)) {
    throw new TypeError(
      `fixture ${task.id} targetProfile does not match tasks.json`,
    );
  }
  requireValidationProfile(
    manifest.validationProfile,
    `fixture ${task.id} validationProfile`,
  );
  if (manifest.validationProfile !== task.validationProfile) {
    throw new TypeError(
      `fixture ${task.id} validationProfile does not match tasks.json`,
    );
  }
  const validationProfile = getValidationProfile(
    manifest.validationProfile,
  );
  if (!fixtureStatuses.has(manifest.status)) {
    throw new TypeError(`fixture ${task.id} has an invalid status`);
  }
  const sandboxBlockReason = sandboxProfileBlockReason(validationProfile);
  if (manifest.status === "active" && sandboxBlockReason) {
    throw new TypeError(
      `fixture ${task.id} must remain a scaffold while ` +
      sandboxBlockReason,
    );
  }
  requireString(manifest.language, "fixture language", languagePattern);

  requireExactFields(manifest.answer, answerFields, "fixture answer");
  if (manifest.answer.format !== "markdown-fenced-code") {
    throw new TypeError(`fixture ${task.id} has an unsupported answer format`);
  }
  requireString(
    manifest.answer.language,
    "fixture answer language",
    languagePattern,
  );
  requireSafeRelativePath(manifest.answer.output, "fixture answer output");

  requireExactFields(manifest.paths, pathFields, "fixture paths");
  for (const [name, path] of Object.entries(manifest.paths)) {
    requireSafeRelativePath(path, `fixture paths.${name}`);
    if (path !== requiredPaths[name]) {
      throw new TypeError(
        `fixture paths.${name} must be ${requiredPaths[name]}`,
      );
    }
  }
  if (new Set(Object.values(manifest.paths)).size !== pathFields.length) {
    throw new TypeError(`fixture ${task.id} paths must be unique`);
  }
  if (!manifest.answer.output.startsWith(`${manifest.paths.generated}/`)) {
    throw new TypeError(
      `fixture ${task.id} answer output must be under generated/`,
    );
  }

  requireObject(manifest.toolVersionArgs, "fixture toolVersionArgs");
  const profileToolchains = new Map(
    validationProfile.toolchains.map((tool) => [
      tool,
      validationProfile.environments.map((reference) =>
        getValidationEnvironmentRevision(
          reference.id,
          reference.revision,
        ).toolchains.find((toolchain) => toolchain.name === tool)),
    ]),
  );
  const configuredTools = new Set();
  for (const [tool, args] of Object.entries(manifest.toolVersionArgs)) {
    if (
      !toolPattern.test(tool) ||
      !Array.isArray(args) ||
      args.length === 0 ||
      args.some((arg) =>
        typeof arg !== "string" ||
        arg.length === 0 ||
        arg.includes("\0"))
    ) {
      throw new TypeError(
        `fixture ${task.id} toolVersionArgs is invalid`,
      );
    }
    const environmentToolchains = profileToolchains.get(tool);
    if (!environmentToolchains) {
      throw new TypeError(
        `fixture ${task.id} tool ${tool} is not in its validation profile`,
      );
    }
    if (
      environmentToolchains.some((profileToolchain) =>
        args.length !== profileToolchain.versionArgs.length ||
        args.some((argument, index) =>
          argument !== profileToolchain.versionArgs[index]))
    ) {
      throw new TypeError(
        `fixture ${task.id} tool ${tool} versionArgs do not match its ` +
        "validation profile",
      );
    }
    configuredTools.add(tool);
  }

  if (!Array.isArray(manifest.commands) || manifest.commands.length === 0) {
    throw new TypeError(`fixture ${task.id} must define commands`);
  }
  const commandIds = new Set();
  const requiredTools = new Set();
  for (const command of manifest.commands) {
    requireExactFields(command, commandFields, "fixture command");
    requireString(command.id, "fixture command id", identifierPattern);
    if (commandIds.has(command.id)) {
      throw new TypeError(`fixture ${task.id} has duplicate command IDs`);
    }
    if (!commandPhases.has(command.phase)) {
      throw new TypeError(`fixture ${task.id} has an invalid command phase`);
    }
    if (!Array.isArray(command.argv) || command.argv.length === 0 ||
        command.argv.some((value) =>
          typeof value !== "string" || value.length === 0 ||
          value.includes("\0"))) {
      throw new TypeError(`fixture ${task.id} command argv is invalid`);
    }
    if (!Array.isArray(command.requiredTools) ||
        command.requiredTools.some((tool) =>
          typeof tool !== "string" || !toolPattern.test(tool))) {
      throw new TypeError(
        `fixture ${task.id} command requiredTools is invalid`,
      );
    }
    if (new Set(command.requiredTools).size !== command.requiredTools.length) {
      throw new TypeError(
        `fixture ${task.id} command requiredTools must be unique`,
      );
    }
    for (const tool of command.requiredTools) requiredTools.add(tool);
    if (command.argv[0].includes("/")) {
      requireSafeRelativePath(
        command.argv[0],
        `fixture ${task.id} command executable`,
      );
      if (
        command.phase !== "test" ||
        !command.argv[0].startsWith(`${manifest.paths.build}/`) ||
        command.requiredTools.length !== 0
      ) {
        throw new TypeError(
          `fixture ${task.id} test executable must be under build/`,
        );
      }
    } else if (
      !toolPattern.test(command.argv[0]) ||
      forbiddenCommandTools.has(command.argv[0]) ||
      !command.requiredTools.includes(command.argv[0])
    ) {
      throw new TypeError(
        `fixture ${task.id} command must invoke a declared non-shell tool`,
      );
    }
    if (
      validationProfile.testRuntime &&
      !validationProfileCommandContract(validationProfile, command)
    ) {
      throw new TypeError(
        `fixture ${task.id} command ${command.id} is not approved by ` +
        `validation profile ${validationProfile.id}`,
      );
    }
    if (!Number.isSafeInteger(command.timeoutMs) || command.timeoutMs < 1) {
      throw new TypeError(`fixture ${task.id} command timeoutMs is invalid`);
    }
    commandIds.add(command.id);
  }
  if (
    configuredTools.size !== requiredTools.size ||
    [...configuredTools].some((tool) => !requiredTools.has(tool))
  ) {
    throw new TypeError(
      `fixture ${task.id} toolVersionArgs must cover requiredTools exactly`,
    );
  }
  if (
    requiredTools.size !== validationProfile.toolchains.length ||
    validationProfile.toolchains.some((tool) => !requiredTools.has(tool))
  ) {
    throw new TypeError(
      `fixture ${task.id} requiredTools must cover its validation profile ` +
      "toolchains exactly",
    );
  }
  if (manifest.status === "active") {
    for (const phase of ["compile", "test"]) {
      if (!manifest.commands.some((command) => command.phase === phase)) {
        throw new TypeError(
          `active fixture ${task.id} must define a ${phase} command`,
        );
      }
    }
  }
  return manifest;
}

function rejectSymlinks(directory, taskId) {
  for (const entry of readdirSync(directory, { withFileTypes: true })) {
    const path = join(directory, entry.name);
    if (entry.isSymbolicLink()) {
      throw new TypeError(`fixture ${taskId} contains a symlink: ${path}`);
    }
    if (entry.isDirectory()) rejectSymlinks(path, taskId);
  }
}

function validateTrackedDirectories(fixtureRoot, manifest) {
  for (const field of trackedDirectoryFields) {
    const path = resolve(fixtureRoot, manifest.paths[field]);
    const relativePath = relative(fixtureRoot, path);
    if (relativePath === ".." ||
        relativePath.startsWith(`..${sep}`) ||
        relativePath === "") {
      throw new TypeError(
        `fixture ${manifest.taskId} ${field} path escapes its directory`,
      );
    }
    if (!existsSync(path) || !lstatSync(path).isDirectory()) {
      throw new TypeError(
        `fixture ${manifest.taskId} is missing ${manifest.paths[field]}`,
      );
    }
    rejectSymlinks(path, manifest.taskId);
  }
}

export function validateFixtureRepository({
  fixturesRoot,
  tasksPath,
}) {
  const root = resolve(
    fixturesRoot instanceof URL ? fileURLToPath(fixturesRoot) : fixturesRoot,
  );
  const tasks = loadTasks(tasksPath);
  const tasksById = new Map(tasks.map((task) => [task.id, task]));
  const requiredTaskIds = new Set(
    tasks.filter((task) => task.targetProfile).map((task) => task.id),
  );
  const fixtureTaskIds = new Set();
  let commandCount = 0;
  let activeCount = 0;
  let scaffoldCount = 0;

  for (const entry of readdirSync(root, { withFileTypes: true })) {
    if (entry.isSymbolicLink()) {
      throw new TypeError(`fixture symlinks are not allowed: ${entry.name}`);
    }
    if (!entry.isDirectory()) continue;
    const task = tasksById.get(entry.name);
    if (!task) {
      throw new TypeError(`fixture directory has no task: ${entry.name}`);
    }
    const taskFixtureRoot = join(root, entry.name);
    const manifestPath = join(taskFixtureRoot, "manifest.json");
    if (!existsSync(manifestPath) || lstatSync(manifestPath).isSymbolicLink()) {
      throw new TypeError(`fixture ${entry.name} is missing manifest.json`);
    }
    const manifest = validateFixtureManifest(
      JSON.parse(readFileSync(manifestPath, "utf8")),
      task,
    );
    validateTrackedDirectories(taskFixtureRoot, manifest);
    fixtureTaskIds.add(entry.name);
    commandCount += manifest.commands.length;
    if (manifest.status === "active") activeCount++;
    if (manifest.status === "scaffold") scaffoldCount++;
  }

  for (const taskId of requiredTaskIds) {
    if (!fixtureTaskIds.has(taskId)) {
      throw new TypeError(`task ${taskId} is missing a fixture directory`);
    }
  }

  return {
    fixtureCount: fixtureTaskIds.size,
    activeCount,
    scaffoldCount,
    commandCount,
  };
}

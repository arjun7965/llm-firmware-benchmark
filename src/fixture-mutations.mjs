import { spawnSync } from "node:child_process";
import {
  cpSync,
  existsSync,
  lstatSync,
  mkdirSync,
  mkdtempSync,
  readFileSync,
  readdirSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import {
  dirname,
  join,
  relative,
  resolve,
  sep,
} from "node:path";
import { fileURLToPath } from "node:url";
import {
  validateFixtureManifest,
  validateFixtureRepository,
} from "./fixtures.mjs";
import { loadTasks } from "./harness.mjs";

const maximumOutputBytes = 1024 * 1024;
const identifierPattern = /^[a-z][a-z0-9-]*$/u;

function asPath(value) {
  return value instanceof URL ? fileURLToPath(value) : value;
}

function requireExactKeys(value, keys, name) {
  if (
    !value ||
    typeof value !== "object" ||
    Array.isArray(value) ||
    Object.keys(value).sort().join(",") !== [...keys].sort().join(",")
  ) {
    throw new TypeError(`${name} has unexpected fields`);
  }
}

function requireSafeRelativePath(value, name) {
  if (
    typeof value !== "string" ||
    value === "" ||
    value.includes("\\") ||
    value.startsWith("/") ||
    value.split("/").some((segment) =>
      segment === "" || segment === "." || segment === "..")
  ) {
    throw new TypeError(`${name} must be a safe relative path`);
  }
}

function requireRegularFile(path, name) {
  if (!existsSync(path)) throw new TypeError(`${name} does not exist`);
  const metadata = lstatSync(path);
  if (metadata.isSymbolicLink() || !metadata.isFile()) {
    throw new TypeError(`${name} must be a non-symlink regular file`);
  }
}

function requireContained(root, path, name) {
  const relativePath = relative(root, path);
  if (
    relativePath === "" ||
    relativePath === ".." ||
    relativePath.startsWith(`..${sep}`)
  ) {
    throw new TypeError(`${name} escapes its fixture`);
  }
}

export function loadMutationCatalog(fixtureRoot, manifest) {
  const catalogPath = join(fixtureRoot, "mutations.json");
  requireRegularFile(catalogPath, `${manifest.taskId} mutation catalog`);
  const catalog = JSON.parse(readFileSync(catalogPath, "utf8"));
  requireExactKeys(
    catalog,
    ["mutations", "schemaVersion", "source"],
    `${manifest.taskId} mutation catalog`,
  );
  if (catalog.schemaVersion !== "1.2") {
    throw new TypeError(`${manifest.taskId} mutation schema is unsupported`);
  }
  requireSafeRelativePath(
    catalog.source,
    `${manifest.taskId} mutation source`,
  );
  if (!catalog.source.startsWith(`${manifest.paths.reference}/`)) {
    throw new TypeError(
      `${manifest.taskId} mutation source must be under reference/`,
    );
  }
  const sourcePath = resolve(fixtureRoot, catalog.source);
  requireContained(fixtureRoot, sourcePath, "mutation source");
  requireRegularFile(sourcePath, `${manifest.taskId} mutation source`);
  if (!Array.isArray(catalog.mutations) || catalog.mutations.length === 0) {
    throw new TypeError(`${manifest.taskId} mutations must be non-empty`);
  }

  const mutationIds = new Set();
  for (const mutation of catalog.mutations) {
    requireExactKeys(
      mutation,
      ["description", "find", "id", "replace"],
      `${manifest.taskId} mutation`,
    );
    if (
      typeof mutation.id !== "string" ||
      !identifierPattern.test(mutation.id) ||
      mutationIds.has(mutation.id)
    ) {
      throw new TypeError(`${manifest.taskId} mutation id is invalid`);
    }
    if (
      typeof mutation.description !== "string" ||
      mutation.description.trim() === "" ||
      typeof mutation.find !== "string" ||
      mutation.find === "" ||
      typeof mutation.replace !== "string" ||
      mutation.find === mutation.replace ||
      mutation.find.includes("\0") ||
      mutation.replace.includes("\0")
    ) {
      throw new TypeError(`${manifest.taskId} mutation is invalid`);
    }
    mutationIds.add(mutation.id);
  }
  return {
    catalog,
    sourcePath,
  };
}

export function applyMutation(source, mutation, taskId) {
  const occurrences = source.split(mutation.find).length - 1;
  if (occurrences !== 1) {
    throw new TypeError(
      `${taskId}/${mutation.id} must match exactly once; found ${occurrences}`,
    );
  }
  return source.replace(mutation.find, mutation.replace);
}

function run(command, args, {
  cwd,
  timeout,
}) {
  return spawnSync(command, args, {
    cwd,
    encoding: "utf8",
    killSignal: "SIGKILL",
    maxBuffer: maximumOutputBytes,
    stdio: ["ignore", "pipe", "pipe"],
    timeout,
  });
}

export function mutationDiagnostics(result) {
  return [
    result.error
      ? `${result.error.code ?? "ERROR"}: ${result.error.message}`
      : "",
    result.stdout ?? "",
    result.stderr ?? "",
  ].filter(Boolean).join("\n").trim();
}

function requireCompleted(result, name) {
  if (result.error || !Number.isInteger(result.status)) {
    throw new Error(`${name} did not complete\n${mutationDiagnostics(result)}`);
  }
}

export function fixtureMutationCommands(manifest) {
  const compile = manifest.commands.filter((command) =>
    command.phase === "compile");
  const test = manifest.commands.filter((command) =>
    command.phase === "test");
  if (compile.length !== 1 || test.length !== 1) {
    throw new TypeError(
      `${manifest.taskId} mutation testing requires one compile and one test command`,
    );
  }
  return {
    compile: compile[0],
    test: test[0],
  };
}

function isDeclaredBuildPath(argument, manifest) {
  return argument.startsWith(`${manifest.paths.build}/`);
}

function declaredBuildPaths(argv, manifest) {
  return argv.filter((argument) => {
    if (!isDeclaredBuildPath(argument, manifest)) return false;
    requireSafeRelativePath(argument, `${manifest.taskId} build artifact`);
    return true;
  });
}

function countArgument(argv, value) {
  return argv.filter((argument) => argument === value).length;
}

function buildPathReplacements(commands, manifest, candidateRoot) {
  const compileBuildPaths = new Set(
    declaredBuildPaths(commands.compile.argv, manifest),
  );
  const testBuildPaths = new Set(
    declaredBuildPaths(commands.test.argv, manifest),
  );
  const sharedBuildPaths = [...testBuildPaths].filter((path) =>
    compileBuildPaths.has(path));
  if (sharedBuildPaths.length === 0) {
    throw new TypeError(
      `${manifest.taskId} compile and test commands must share a build artifact`,
    );
  }

  const replacements = new Map();
  for (const buildPath of new Set([
    ...compileBuildPaths,
    ...testBuildPaths,
  ])) {
    const replacement = join(candidateRoot, buildPath);
    mkdirSync(dirname(replacement), { recursive: true });
    replacements.set(buildPath, replacement);
  }
  return replacements;
}

function fixtureInputReplacements(commands, manifest, candidateRoot) {
  const replacements = new Map();
  const roots = ["starter", "mocks", "publicTests"]
    .map((field) => manifest.paths[field])
    .filter((path) => typeof path === "string");
  for (const argument of [
    ...commands.compile.argv,
    ...commands.test.argv,
  ]) {
    if (!roots.some((root) =>
      argument === root || argument.startsWith(`${root}/`))) {
      continue;
    }
    requireSafeRelativePath(argument, `${manifest.taskId} fixture input`);
    replacements.set(argument, join(candidateRoot, argument));
  }
  return replacements;
}

function replaceArgv(argv, replacements) {
  return argv.map((argument) => replacements.get(argument) ?? argument);
}

export function createMutationCommandPlan({
  candidatePath,
  candidateRoot,
  commands,
  manifest,
}) {
  const answerOccurrences = countArgument(
    commands.compile.argv,
    manifest.answer.output,
  );
  const inputReplacements = fixtureInputReplacements(
    commands,
    manifest,
    candidateRoot,
  );
  const compileUsesFixtureInput = commands.compile.argv.some((argument) =>
    inputReplacements.has(argument));
  if (
    answerOccurrences > 1 ||
    (answerOccurrences === 0 && !compileUsesFixtureInput)
  ) {
    throw new TypeError(
      `${manifest.taskId} compile command cannot be adapted for mutations`,
    );
  }

  const replacements = buildPathReplacements(
    commands,
    manifest,
    candidateRoot,
  );
  for (const [path, replacement] of inputReplacements) {
    replacements.set(path, replacement);
  }
  replacements.set(manifest.answer.output, candidatePath);

  const compileArgv = replaceArgv(commands.compile.argv, replacements);
  const testArgv = replaceArgv(commands.test.argv, replacements);

  return {
    compile: {
      args: compileArgv.slice(1),
      command: compileArgv[0],
      timeoutMs: commands.compile.timeoutMs,
    },
    test: {
      args: testArgv.slice(1),
      command: testArgv[0],
      timeoutMs: commands.test.timeoutMs,
    },
  };
}

function stageMutationCandidate({
  candidateRoot,
  fixtureRoot,
  manifest,
  source,
}) {
  for (const field of ["starter", "mocks", "publicTests"]) {
    const relativePath = manifest.paths[field];
    const destination = join(candidateRoot, relativePath);
    mkdirSync(dirname(destination), { recursive: true });
    cpSync(join(fixtureRoot, relativePath), destination, {
      recursive: true,
    });
  }
  const candidatePath = join(candidateRoot, manifest.answer.output);
  mkdirSync(dirname(candidatePath), { recursive: true });
  writeFileSync(candidatePath, source, { encoding: "utf8", mode: 0o600 });
  return candidatePath;
}

function compileCandidate({
  commandPlan,
  fixtureRoot,
  manifest,
  name,
}) {
  const result = run(commandPlan.compile.command, commandPlan.compile.args, {
    cwd: fixtureRoot,
    timeout: commandPlan.compile.timeoutMs,
  });
  requireCompleted(result, `${name} compilation`);
  if (result.status !== 0) {
    throw new Error(
      `${name} did not compile; mutations must remain compile-valid ` +
      `for ${manifest.language}\n${mutationDiagnostics(result)}`,
    );
  }
}

function runCandidate({
  commandPlan,
  fixtureRoot,
  name,
}) {
  const result = run(commandPlan.test.command, commandPlan.test.args, {
    cwd: fixtureRoot,
    timeout: commandPlan.test.timeoutMs,
  });
  requireCompleted(result, `${name} tests`);
  return result;
}

export function runFixtureMutationTests({
  fixtureStatuses = ["active"],
  fixturesRoot,
  logger = console.log,
  taskIds = null,
  tasksPath,
  temporaryRoot,
}) {
  if (
    !Array.isArray(fixtureStatuses) ||
    fixtureStatuses.length === 0 ||
    fixtureStatuses.some((status) =>
      !["active", "scaffold"].includes(status)) ||
    new Set(fixtureStatuses).size !== fixtureStatuses.length
  ) {
    throw new TypeError("fixtureStatuses must be unique known statuses");
  }
  if (
    taskIds !== null &&
    (
      !Array.isArray(taskIds) ||
      taskIds.length === 0 ||
      taskIds.some((taskId) =>
        typeof taskId !== "string" || !identifierPattern.test(taskId)) ||
      new Set(taskIds).size !== taskIds.length
    )
  ) {
    throw new TypeError("taskIds must be unique fixture task IDs");
  }
  const fixtureStatusSet = new Set(fixtureStatuses);
  const taskIdSet = taskIds === null ? null : new Set(taskIds);
  const resolvedFixturesRoot = resolve(asPath(fixturesRoot));
  const resolvedTasksPath = resolve(asPath(tasksPath));
  validateFixtureRepository({
    fixturesRoot: resolvedFixturesRoot,
    tasksPath: resolvedTasksPath,
  });
  const tasksById = new Map(
    loadTasks(resolvedTasksPath).map((task) => [task.id, task]),
  );
  const resolvedTemporaryRoot = temporaryRoot
    ? resolve(asPath(temporaryRoot))
    : mkdtempSync(join(tmpdir(), "fixture-mutation-tests-"));
  if (temporaryRoot) mkdirSync(resolvedTemporaryRoot, { recursive: true });

  let killedMutations = 0;
  let mutationFixtures = 0;

  try {
    const fixtureNames = readdirSync(
      resolvedFixturesRoot,
      { withFileTypes: true },
    )
      .filter((entry) => entry.isDirectory())
      .map((entry) => entry.name)
      .sort();

    for (const fixtureName of fixtureNames) {
      const fixtureRoot = join(resolvedFixturesRoot, fixtureName);
      const manifestPath = join(fixtureRoot, "manifest.json");
      if (!existsSync(manifestPath)) continue;
      requireRegularFile(manifestPath, `${fixtureName} manifest`);
      const task = tasksById.get(fixtureName);
      if (!task) throw new TypeError(`unknown fixture task: ${fixtureName}`);
      const manifest = validateFixtureManifest(
        JSON.parse(readFileSync(manifestPath, "utf8")),
        task,
      );
      if (
        !fixtureStatusSet.has(manifest.status) ||
        (taskIdSet !== null && !taskIdSet.has(manifest.taskId))
      ) {
        continue;
      }
      mutationFixtures++;

      const { catalog, sourcePath } = loadMutationCatalog(
        fixtureRoot,
        manifest,
      );
      const commands = fixtureMutationCommands(manifest);
      const source = readFileSync(sourcePath, "utf8");
      const fixtureTemporaryRoot = join(resolvedTemporaryRoot, fixtureName);
      mkdirSync(fixtureTemporaryRoot);

      const baselineRoot = join(fixtureTemporaryRoot, "_baseline");
      mkdirSync(baselineRoot);
      const baselinePath = stageMutationCandidate({
        candidateRoot: baselineRoot,
        fixtureRoot,
        manifest,
        source,
      });
      const baselinePlan = createMutationCommandPlan({
        candidatePath: baselinePath,
        candidateRoot: baselineRoot,
        commands,
        manifest,
      });
      compileCandidate({
        commandPlan: baselinePlan,
        fixtureRoot,
        manifest,
        name: `${fixtureName} baseline`,
      });
      const baselineResult = runCandidate({
        commandPlan: baselinePlan,
        fixtureRoot,
        name: `${fixtureName} baseline`,
      });
      if (baselineResult.status !== 0) {
        throw new Error(
          `${fixtureName} trusted baseline failed\n` +
          mutationDiagnostics(baselineResult),
        );
      }
      logger(`ok - ${fixtureName}/baseline`);

      for (const mutation of catalog.mutations) {
        const name = `${fixtureName}/${mutation.id}`;
        const candidateRoot = join(fixtureTemporaryRoot, mutation.id);
        mkdirSync(candidateRoot);
        const candidatePath = stageMutationCandidate({
          candidateRoot,
          fixtureRoot,
          manifest,
          source: applyMutation(source, mutation, fixtureName),
        });
        const commandPlan = createMutationCommandPlan({
          candidatePath,
          candidateRoot,
          commands,
          manifest,
        });
        compileCandidate({
          commandPlan,
          fixtureRoot,
          manifest,
          name,
        });
        const result = runCandidate({
          commandPlan,
          fixtureRoot,
          name,
        });
        if (result.status === 0) {
          throw new Error(
            `${name} survived: ${mutation.description}`,
          );
        }
        killedMutations++;
        logger(`ok - ${name}`);
      }
    }

    if (mutationFixtures === 0) {
      throw new Error("no selected fixtures found for mutation testing");
    }

    logger(
      `Fixture mutation testing passed: ${killedMutations} mutations killed.`,
    );
    return {
      fixtureCount: mutationFixtures,
      killedMutations,
    };
  } finally {
    if (!temporaryRoot) {
      rmSync(resolvedTemporaryRoot, { recursive: true, force: true });
    }
  }
}

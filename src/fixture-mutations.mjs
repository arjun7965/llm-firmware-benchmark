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
  symlinkSync,
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
import { attestDependencyInstallation } from "./dependency-installation.mjs";
import { getValidationProfile } from "./validation-profiles.mjs";
import { runLocalPostgresqlCommand } from "./postgresql-service.mjs";

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
  const suppliedInputMode = Object.hasOwn(catalog, "answerSource");
  requireExactKeys(
    catalog,
    suppliedInputMode
      ? [
        "answerSource",
        "mutations",
        "schemaVersion",
        "source",
        "stagedPath",
      ]
      : ["mutations", "schemaVersion", "source"],
    `${manifest.taskId} mutation catalog`,
  );
  if (!["1.2", "1.3"].includes(catalog.schemaVersion)) {
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
  let answerSourcePath = null;
  if (suppliedInputMode) {
    requireSafeRelativePath(
      catalog.answerSource,
      `${manifest.taskId} mutation answerSource`,
    );
    if (!catalog.answerSource.startsWith(`${manifest.paths.reference}/`)) {
      throw new TypeError(
        `${manifest.taskId} mutation answerSource must be under reference/`,
      );
    }
    answerSourcePath = resolve(fixtureRoot, catalog.answerSource);
    requireContained(fixtureRoot, answerSourcePath, "mutation answerSource");
    requireRegularFile(
      answerSourcePath,
      `${manifest.taskId} mutation answerSource`,
    );
    requireSafeRelativePath(
      catalog.stagedPath,
      `${manifest.taskId} mutation stagedPath`,
    );
    if (![manifest.paths.starter, manifest.paths.mocks].some((root) =>
      catalog.stagedPath.startsWith(`${root}/`))) {
      throw new TypeError(
        `${manifest.taskId} mutation stagedPath must be under starter/ or mocks/`,
      );
    }
  }
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
    answerSourcePath,
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
  validationProfile,
  timeout,
}) {
  if (validationProfile.testRuntime?.service?.kind === "postgresql") {
    return runLocalPostgresqlCommand({
      args,
      command,
      cwd,
      profile: validationProfile,
      timeoutMs: timeout,
    });
  }
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
  if (compile.length === 0 || test.length !== 1) {
    throw new TypeError(
      `${manifest.taskId} mutation testing requires compile commands and ` +
      "one test command",
    );
  }
  return {
    compile,
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

function answerOutputPaths(manifest) {
  if (typeof manifest.answer.output === "string") {
    return [manifest.answer.output];
  }
  if (
    manifest.answer.format === "markdown-file-bundle" &&
    Array.isArray(manifest.answer.files)
  ) {
    return manifest.answer.files.map((file) =>
      `${manifest.paths.generated}/${file.path}`);
  }
  throw new TypeError(`${manifest.taskId} answer contract is invalid`);
}

function buildPathReplacements(commands, manifest, candidateRoot) {
  const compileBuildPaths = new Set(
    commands.compile.flatMap((command) =>
      declaredBuildPaths(command.argv, manifest)),
  );
  const testBuildPaths = new Set(
    declaredBuildPaths(commands.test.argv, manifest),
  );
  const hasSharedBuildOutput = [...compileBuildPaths].some((compilePath) =>
    [...testBuildPaths].some((testPath) =>
      testPath === compilePath || testPath.startsWith(`${compilePath}/`)));
  if (!hasSharedBuildOutput) {
    if (compileBuildPaths.size === 0 && testBuildPaths.size === 0) {
      return new Map();
    }
    throw new TypeError(
      `${manifest.taskId} compile and test commands must share a build output`,
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
    ...commands.compile.flatMap((command) => command.argv),
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
  const compileArgv = commands.compile.flatMap((command) => command.argv);
  const answerOutputs = answerOutputPaths(manifest);
  const primaryAnswerOutput = answerOutputs[0];
  const answerOccurrences = countArgument(
    compileArgv,
    primaryAnswerOutput,
  );
  const inputReplacements = fixtureInputReplacements(
    commands,
    manifest,
    candidateRoot,
  );
  const compileUsesFixtureInput = compileArgv.some((argument) =>
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
  replacements.set(primaryAnswerOutput, candidatePath);
  for (const output of answerOutputs.slice(1)) {
    replacements.set(output, join(candidateRoot, output));
  }

  const testArgv = replaceArgv(commands.test.argv, replacements);

  return {
    compile: commands.compile.map((command) => {
      const argv = replaceArgv(command.argv, replacements);
      return {
        args: argv.slice(1),
        command: argv[0],
        timeoutMs: command.timeoutMs,
      };
    }),
    test: {
      args: testArgv.slice(1),
      command: testArgv[0],
      timeoutMs: commands.test.timeoutMs,
    },
  };
}

function stageMutationCandidate({
  answerSourcePath,
  catalog,
  candidateRoot,
  dependencyInstallRoot,
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
  if (dependencyInstallRoot !== null) {
    symlinkSync(
      dependencyInstallRoot,
      join(candidateRoot, "node_modules"),
      "dir",
    );
  }
  const answerOutputs = answerOutputPaths(manifest);
  const candidatePath = join(candidateRoot, answerOutputs[0]);
  mkdirSync(dirname(candidatePath), { recursive: true });
  if (answerSourcePath !== null) {
    const stagedPath = join(candidateRoot, catalog.stagedPath);
    mkdirSync(dirname(stagedPath), { recursive: true });
    writeFileSync(stagedPath, source, { encoding: "utf8", mode: 0o600 });
    writeFileSync(
      candidatePath,
      readFileSync(answerSourcePath),
      { mode: 0o600 },
    );
  } else {
    writeFileSync(candidatePath, source, { encoding: "utf8", mode: 0o600 });
  }
  if (manifest.answer.format === "markdown-file-bundle") {
    for (let index = 1; index < answerOutputs.length; index++) {
      const outputPath = join(candidateRoot, answerOutputs[index]);
      const referencePath = join(
        fixtureRoot,
        manifest.paths.reference,
        manifest.answer.files[index].path,
      );
      requireRegularFile(
        referencePath,
        `${manifest.taskId} reference answer bundle file`,
      );
      mkdirSync(dirname(outputPath), { recursive: true });
      writeFileSync(outputPath, readFileSync(referencePath), { mode: 0o600 });
    }
  }
  return candidatePath;
}

function compileCandidate({
  commandPlan,
  fixtureRoot,
  manifest,
  name,
  validationProfile,
}) {
  for (const compile of commandPlan.compile) {
    const result = run(compile.command, compile.args, {
      cwd: fixtureRoot,
      validationProfile,
      timeout: compile.timeoutMs,
    });
    requireCompleted(result, `${name} compilation`);
    if (result.status !== 0) {
      throw new Error(
        `${name} did not compile; mutations must remain compile-valid ` +
        `for ${manifest.language}\n${mutationDiagnostics(result)}`,
      );
    }
  }
}

function runCandidate({
  commandPlan,
  fixtureRoot,
  name,
  validationProfile,
}) {
  const result = run(commandPlan.test.command, commandPlan.test.args, {
    cwd: fixtureRoot,
    validationProfile,
    timeout: commandPlan.test.timeoutMs,
  });
  requireCompleted(result, `${name} tests`);
  return result;
}

export function runFixtureMutationTests({
  attestDependencyInstallationImpl = attestDependencyInstallation,
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
      const validationProfile = getValidationProfile(
        manifest.validationProfile,
      );
      const dependencyInstallRoot =
        validationProfile.dependencyInstall?.source === "npm" &&
          Object.hasOwn(
            validationProfile.dependencyInstall,
            "installRoot",
          )
          ? attestDependencyInstallationImpl(validationProfile).installRoot
          : null;

      const { answerSourcePath, catalog, sourcePath } = loadMutationCatalog(
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
        answerSourcePath,
        catalog,
        candidateRoot: baselineRoot,
        dependencyInstallRoot,
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
        validationProfile,
      });
      const baselineResult = runCandidate({
        commandPlan: baselinePlan,
        fixtureRoot,
        name: `${fixtureName} baseline`,
        validationProfile,
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
          answerSourcePath,
          catalog,
          candidateRoot,
          dependencyInstallRoot,
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
          validationProfile,
        });
        const result = runCandidate({
          commandPlan,
          fixtureRoot,
          name,
          validationProfile,
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

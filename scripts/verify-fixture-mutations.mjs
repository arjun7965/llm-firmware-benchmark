import { spawnSync } from "node:child_process";
import {
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
  join,
  relative,
  resolve,
  sep,
} from "node:path";
import { fileURLToPath } from "node:url";
import {
  validateFixtureManifest,
  validateFixtureRepository,
} from "../src/fixtures.mjs";
import { loadTasks } from "../src/harness.mjs";

const repositoryRoot = fileURLToPath(new URL("../", import.meta.url));
const fixturesRoot = join(repositoryRoot, "fixtures");
const tasksPath = join(repositoryRoot, "tasks.json");
const maximumOutputBytes = 1024 * 1024;
const identifierPattern = /^[a-z][a-z0-9-]*$/u;

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

function loadMutationCatalog(fixtureRoot, manifest) {
  const catalogPath = join(fixtureRoot, "mutations.json");
  requireRegularFile(catalogPath, `${manifest.taskId} mutation catalog`);
  const catalog = JSON.parse(readFileSync(catalogPath, "utf8"));
  requireExactKeys(
    catalog,
    ["mutations", "schemaVersion", "source"],
    `${manifest.taskId} mutation catalog`,
  );
  if (catalog.schemaVersion !== "1.0") {
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

function applyMutation(source, mutation, taskId) {
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

function diagnostics(result) {
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
    throw new Error(`${name} did not complete\n${diagnostics(result)}`);
  }
}

function fixtureCommands(manifest) {
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

function compileCandidate({
  binaryPath,
  candidatePath,
  commands,
  fixtureRoot,
  manifest,
  name,
}) {
  let replacedAnswer = false;
  let replacedBinary = false;
  const args = commands.compile.argv.slice(1).map((argument) => {
    if (argument === manifest.answer.output) {
      replacedAnswer = true;
      return candidatePath;
    }
    if (argument === commands.test.argv[0]) {
      replacedBinary = true;
      return binaryPath;
    }
    return argument;
  });
  if (!replacedAnswer || !replacedBinary) {
    throw new TypeError(
      `${manifest.taskId} compile command cannot be adapted for mutations`,
    );
  }
  const result = run(commands.compile.argv[0], args, {
    cwd: fixtureRoot,
    timeout: commands.compile.timeoutMs,
  });
  requireCompleted(result, `${name} compilation`);
  if (result.status !== 0) {
    throw new Error(
      `${name} did not compile; mutations must remain valid C\n` +
      diagnostics(result),
    );
  }
}

function runCandidate({
  binaryPath,
  commands,
  fixtureRoot,
  name,
}) {
  const result = run(binaryPath, commands.test.argv.slice(1), {
    cwd: fixtureRoot,
    timeout: commands.test.timeoutMs,
  });
  requireCompleted(result, `${name} tests`);
  return result;
}

validateFixtureRepository({
  fixturesRoot,
  tasksPath,
});
const tasksById = new Map(
  loadTasks(tasksPath).map((task) => [task.id, task]),
);
const temporaryRoot = mkdtempSync(
  join(tmpdir(), "fixture-mutation-tests-"),
);
let killedMutations = 0;

try {
  const fixtureNames = readdirSync(fixturesRoot, { withFileTypes: true })
    .filter((entry) => entry.isDirectory())
    .map((entry) => entry.name)
    .sort();
  let activeCFixtures = 0;

  for (const fixtureName of fixtureNames) {
    const fixtureRoot = join(fixturesRoot, fixtureName);
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
      manifest.status !== "active" ||
      manifest.answer.language !== "c"
    ) {
      continue;
    }
    activeCFixtures++;

    const { catalog, sourcePath } = loadMutationCatalog(
      fixtureRoot,
      manifest,
    );
    const commands = fixtureCommands(manifest);
    const source = readFileSync(sourcePath, "utf8");
    const fixtureTemporaryRoot = join(temporaryRoot, fixtureName);
    mkdirSync(fixtureTemporaryRoot);

    const baselineBinary = join(fixtureTemporaryRoot, "baseline-tests");
    compileCandidate({
      binaryPath: baselineBinary,
      candidatePath: sourcePath,
      commands,
      fixtureRoot,
      manifest,
      name: `${fixtureName} baseline`,
    });
    const baselineResult = runCandidate({
      binaryPath: baselineBinary,
      commands,
      fixtureRoot,
      name: `${fixtureName} baseline`,
    });
    if (baselineResult.status !== 0) {
      throw new Error(
        `${fixtureName} trusted baseline failed\n` +
        diagnostics(baselineResult),
      );
    }
    console.log(`ok - ${fixtureName}/baseline`);

    for (const mutation of catalog.mutations) {
      const name = `${fixtureName}/${mutation.id}`;
      const candidatePath = join(
        fixtureTemporaryRoot,
        `${mutation.id}.c`,
      );
      const binaryPath = join(
        fixtureTemporaryRoot,
        `${mutation.id}-tests`,
      );
      writeFileSync(
        candidatePath,
        applyMutation(source, mutation, fixtureName),
        { encoding: "utf8", mode: 0o600 },
      );
      compileCandidate({
        binaryPath,
        candidatePath,
        commands,
        fixtureRoot,
        manifest,
        name,
      });
      const result = runCandidate({
        binaryPath,
        commands,
        fixtureRoot,
        name,
      });
      if (result.status === 0) {
        throw new Error(
          `${name} survived: ${mutation.description}`,
        );
      }
      killedMutations++;
      console.log(`ok - ${name}`);
    }
  }

  if (activeCFixtures === 0) {
    throw new Error("no active C fixtures found for mutation testing");
  }
  console.log(
    `Fixture mutation testing passed: ${killedMutations} mutations killed.`,
  );
} finally {
  rmSync(temporaryRoot, { recursive: true, force: true });
}

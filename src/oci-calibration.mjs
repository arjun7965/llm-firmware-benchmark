import {
  cpSync,
  existsSync,
  mkdirSync,
  mkdtempSync,
  readFileSync,
  readdirSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import {
  basename,
  dirname,
  join,
  resolve,
} from "node:path";
import { fileURLToPath } from "node:url";
import { runFixtureValidation } from "./fixture-sandbox.mjs";
import {
  fixtureAnswerFiles,
  validateFixtureManifest,
  validateFixtureRepository,
} from "./fixtures.mjs";
import {
  applyMutation,
  loadMutationCatalog,
} from "./fixture-mutations.mjs";
import { loadTasks } from "./harness.mjs";

const taskIdPattern = /^[a-z][a-z0-9-]*$/u;
export const c11OciValidationEnvironment =
  "debian-12-x86-64-c11-oci";

function asPath(value) {
  return value instanceof URL ? fileURLToPath(value) : value;
}

function requirePositiveInteger(value, name) {
  if (!Number.isSafeInteger(value) || value < 1) {
    throw new TypeError(`${name} must be a positive safe integer`);
  }
}

function requireShard(shardIndex, shardCount) {
  requirePositiveInteger(shardCount, "OCI calibration shardCount");
  if (
    !Number.isSafeInteger(shardIndex) ||
    shardIndex < 0 ||
    shardIndex >= shardCount
  ) {
    throw new TypeError("OCI calibration shardIndex is invalid");
  }
}

export function discoverC11CalibrationTasks({ fixturesRoot, tasksPath }) {
  const resolvedFixturesRoot = resolve(asPath(fixturesRoot));
  const resolvedTasksPath = resolve(asPath(tasksPath));
  validateFixtureRepository({
    fixturesRoot: resolvedFixturesRoot,
    tasksPath: resolvedTasksPath,
  });
  const tasksById = new Map(
    loadTasks(resolvedTasksPath).map((task) => [task.id, task]),
  );
  const selected = [];
  for (const entry of readdirSync(resolvedFixturesRoot, {
    withFileTypes: true,
  }).filter((item) => item.isDirectory()).sort((left, right) =>
    left.name.localeCompare(right.name))) {
    const fixtureRoot = join(resolvedFixturesRoot, entry.name);
    const manifestPath = join(fixtureRoot, "manifest.json");
    const mutationPath = join(fixtureRoot, "mutations.json");
    if (!existsSync(manifestPath) || !existsSync(mutationPath)) continue;
    const task = tasksById.get(entry.name);
    if (!task) throw new TypeError(`unknown fixture task: ${entry.name}`);
    const manifest = validateFixtureManifest(
      JSON.parse(readFileSync(manifestPath, "utf8")),
      task,
    );
    if (
      manifest.status !== "active" ||
      manifest.validationProfile !== "c11-host"
    ) {
      continue;
    }
    const { catalog } = loadMutationCatalog(fixtureRoot, manifest);
    selected.push({
      mutationCount: catalog.mutations.length,
      taskId: manifest.taskId,
    });
  }
  if (selected.length === 0) {
    throw new TypeError("no active C11 fixtures are available for calibration");
  }
  return selected;
}

function copyFixture(sourceRoot, destinationRoot) {
  cpSync(sourceRoot, destinationRoot, {
    recursive: true,
    filter: (path) => !["build", "generated"].includes(basename(path)),
  });
}

function writeCandidate(path, content) {
  mkdirSync(dirname(path), { recursive: true, mode: 0o700 });
  writeFileSync(path, content, { encoding: "utf8", mode: 0o600 });
}

export function stageOciCalibrationCandidate({
  destinationFixturesRoot,
  fixturesRoot,
  mutationId = null,
  taskId,
}) {
  if (typeof taskId !== "string" || !taskIdPattern.test(taskId)) {
    throw new TypeError("OCI calibration taskId is invalid");
  }
  if (
    mutationId !== null &&
    (typeof mutationId !== "string" || !taskIdPattern.test(mutationId))
  ) {
    throw new TypeError("OCI calibration mutationId is invalid");
  }
  const sourceRoot = resolve(asPath(fixturesRoot), taskId);
  const destinationRoot = resolve(asPath(destinationFixturesRoot), taskId);
  rmSync(destinationRoot, { recursive: true, force: true });
  mkdirSync(dirname(destinationRoot), { recursive: true, mode: 0o700 });
  copyFixture(sourceRoot, destinationRoot);

  const manifest = JSON.parse(
    readFileSync(join(sourceRoot, "manifest.json"), "utf8"),
  );
  const {
    answerSourcePath,
    catalog,
    sourcePath,
  } = loadMutationCatalog(sourceRoot, manifest);
  const mutation = mutationId === null
    ? null
    : catalog.mutations.find(({ id }) => id === mutationId);
  if (mutationId !== null && !mutation) {
    throw new TypeError(`${taskId} mutation is unknown: ${mutationId}`);
  }
  const source = readFileSync(sourcePath, "utf8");
  const candidateSource = mutation === null
    ? source
    : applyMutation(source, mutation, taskId);
  const answerFiles = fixtureAnswerFiles(manifest);
  const primaryAnswer = answerSourcePath === null
    ? candidateSource
    : readFileSync(answerSourcePath, "utf8");
  writeCandidate(
    join(destinationRoot, answerFiles[0].path),
    primaryAnswer,
  );

  if (answerSourcePath !== null) {
    writeCandidate(
      join(destinationRoot, catalog.stagedPath),
      candidateSource,
    );
  }
  if (manifest.answer.format === "markdown-file-bundle") {
    for (let index = 1; index < answerFiles.length; index++) {
      writeCandidate(
        join(destinationRoot, answerFiles[index].path),
        readFileSync(join(
          sourceRoot,
          manifest.paths.reference,
          manifest.answer.files[index].path,
        ), "utf8"),
      );
    }
  }
  return {
    destinationRoot,
    manifest,
    mutation,
  };
}

function reportDiagnostics(report) {
  return report.phases.map((phase) => [
    `${phase.id}: ${phase.outcome}`,
    phase.error ?? "",
    phase.stdout,
    phase.stderr,
  ].filter(Boolean).join("\n")).join("\n");
}

export function validateOciCalibrationReport(report, {
  mutationId = null,
} = {}) {
  if (!report || typeof report !== "object" || !Array.isArray(report.phases)) {
    throw new TypeError("OCI calibration report is invalid");
  }
  if (
    report.validationEnvironment?.id !== c11OciValidationEnvironment ||
    report.validationEnvironment?.execution?.kind !== "oci"
  ) {
    throw new Error("OCI calibration used the wrong validation environment");
  }
  if (mutationId === null) {
    if (!report.success) {
      throw new Error(
        `${report.taskId} trusted reference failed OCI calibration\n` +
        reportDiagnostics(report),
      );
    }
    return report;
  }
  const compilePhases = report.phases.filter(({ phase }) => phase === "compile");
  const testPhases = report.phases.filter(({ phase }) => phase === "test");
  if (
    report.success ||
    compilePhases.length === 0 ||
    compilePhases.some(({ outcome }) => outcome !== "passed") ||
    testPhases.length !== 1 ||
    testPhases[0].outcome !== "failed" ||
    testPhases[0].timedOut ||
    testPhases[0].error !== null
  ) {
    throw new Error(
      `${report.taskId}/${mutationId} was not compile-valid and killed ` +
      `by the OCI public tests\n${reportDiagnostics(report)}`,
    );
  }
  return report;
}

export function runOciC11Calibration({
  fixturesRoot,
  logger = console.log,
  runFixtureValidationImpl = runFixtureValidation,
  shardCount = 1,
  shardIndex = 0,
  tasksPath,
  temporaryRoot = null,
}) {
  requireShard(shardIndex, shardCount);
  const allTasks = discoverC11CalibrationTasks({ fixturesRoot, tasksPath });
  const selectedTasks = allTasks.filter((_, index) =>
    index % shardCount === shardIndex);
  if (selectedTasks.length === 0) {
    throw new TypeError("OCI calibration shard selects no fixtures");
  }
  const ownsTemporaryRoot = temporaryRoot === null;
  const calibrationRoot = ownsTemporaryRoot
    ? mkdtempSync(join(tmpdir(), "oci-c11-calibration-"))
    : resolve(asPath(temporaryRoot));
  const destinationFixturesRoot = join(calibrationRoot, "fixtures");
  mkdirSync(destinationFixturesRoot, { recursive: true, mode: 0o700 });
  let killedMutations = 0;

  try {
    for (const { taskId } of selectedTasks) {
      const sourceRoot = resolve(asPath(fixturesRoot), taskId);
      const sourceManifest = JSON.parse(
        readFileSync(join(sourceRoot, "manifest.json"), "utf8"),
      );
      const { catalog } = loadMutationCatalog(sourceRoot, sourceManifest);
      stageOciCalibrationCandidate({
        destinationFixturesRoot,
        fixturesRoot,
        taskId,
      });
      const baseline = runFixtureValidationImpl({
        fixturesRoot: destinationFixturesRoot,
        taskId,
        tasksPath,
        validationEnvironmentId: c11OciValidationEnvironment,
      });
      validateOciCalibrationReport(baseline.report);
      logger(`ok - ${taskId}/baseline (${c11OciValidationEnvironment})`);

      for (const mutation of catalog.mutations) {
        stageOciCalibrationCandidate({
          destinationFixturesRoot,
          fixturesRoot,
          mutationId: mutation.id,
          taskId,
        });
        const result = runFixtureValidationImpl({
          fixturesRoot: destinationFixturesRoot,
          taskId,
          tasksPath,
          validationEnvironmentId: c11OciValidationEnvironment,
        });
        validateOciCalibrationReport(result.report, {
          mutationId: mutation.id,
        });
        killedMutations++;
        logger(`ok - ${taskId}/${mutation.id}`);
      }
    }
    logger(
      `OCI C11 calibration shard ${shardIndex + 1}/${shardCount} passed: ` +
      `${selectedTasks.length} fixtures, ${killedMutations} mutations killed.`,
    );
    return {
      fixtureCount: selectedTasks.length,
      killedMutations,
      shardCount,
      shardIndex,
    };
  } finally {
    if (ownsTemporaryRoot) {
      rmSync(calibrationRoot, { recursive: true, force: true });
    }
  }
}

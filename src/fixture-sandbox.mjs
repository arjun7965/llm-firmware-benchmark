import {
  createHash,
  randomUUID,
} from "node:crypto";
import { spawnSync } from "node:child_process";
import {
  accessSync,
  constants,
  existsSync,
  lstatSync,
  mkdirSync,
  mkdtempSync,
  readFileSync,
  realpathSync,
  renameSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import {
  basename,
  delimiter,
  dirname,
  join,
  relative,
  resolve,
  sep,
} from "node:path";
import { fileURLToPath } from "node:url";
import { validateFixtureManifest } from "./fixtures.mjs";
import { loadTasks } from "./harness.mjs";
import { targetProfileSet } from "./target-profiles.mjs";

const taskIdPattern = /^[a-z0-9]+(?:-[a-z0-9]+)*$/;
const maximumOutputBytes = 1024 * 1024;
const sandboxRoot = "/workspace";
const rootTmpfsBytes = 32 * 1024 * 1024;
const temporaryDirectoryBytes = 16 * 1024 * 1024;

export const sandboxResourceLimits = Object.freeze({
  compile: Object.freeze({
    addressSpaceBytes: 512 * 1024 * 1024,
    cpuSeconds: 15,
    fileBytes: 16 * 1024 * 1024,
    openFiles: 64,
  }),
  test: Object.freeze({
    addressSpaceBytes: 128 * 1024 * 1024,
    cpuSeconds: 3,
    fileBytes: 1024 * 1024,
    openFiles: 32,
  }),
});

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

function requireDateTime(value, name) {
  if (
    typeof value !== "string" ||
    !/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d+)?(?:Z|[+-]\d{2}:\d{2})$/u
      .test(value) ||
    Number.isNaN(Date.parse(value))
  ) {
    throw new TypeError(`${name} must be an ISO date-time`);
  }
}

function requirePositiveInteger(value, name) {
  if (!Number.isSafeInteger(value) || value < 1) {
    throw new TypeError(`${name} must be a positive safe integer`);
  }
}

function requireDirectory(path, name) {
  if (!existsSync(path)) throw new TypeError(`${name} does not exist`);
  const metadata = lstatSync(path);
  if (metadata.isSymbolicLink() || !metadata.isDirectory()) {
    throw new TypeError(`${name} must be a non-symlink directory`);
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
    throw new TypeError(`${name} escapes its expected directory`);
  }
}

function ensureSafeDirectory(root, path) {
  requireContained(root, path, "validation report directory");
  const relativePath = relative(root, path);
  let current = root;
  for (const segment of relativePath.split(sep)) {
    current = join(current, segment);
    if (!existsSync(current)) {
      mkdirSync(current);
      continue;
    }
    const metadata = lstatSync(current);
    if (metadata.isSymbolicLink() || !metadata.isDirectory()) {
      throw new TypeError("validation report directory is unsafe");
    }
  }
}

export function resolveExecutable(name, {
  pathValue = process.env.PATH ?? "",
} = {}) {
  if (
    typeof name !== "string" ||
    !/^[a-z0-9][a-z0-9+._-]*$/u.test(name)
  ) {
    throw new TypeError(`invalid executable name: ${name}`);
  }
  for (const directory of pathValue.split(delimiter)) {
    if (directory === "") continue;
    const candidate = resolve(directory, name);
    try {
      accessSync(candidate, constants.X_OK);
      const realPath = realpathSync(candidate);
      const metadata = lstatSync(realPath);
      if (
        metadata.isFile() &&
        metadata.uid === 0 &&
        (metadata.mode & 0o022) === 0 &&
        (realPath === "/usr" || realPath.startsWith("/usr/"))
      ) {
        return realPath;
      }
    } catch {
      // Continue searching PATH.
    }
  }
  throw new TypeError(`required executable not found: ${name}`);
}

function existingSystemMounts(phase) {
  const candidates = phase === "compile"
    ? ["/usr", "/bin", "/lib", "/lib64"]
    : ["/lib", "/lib64"];
  return candidates
    .filter((path) => existsSync(path));
}

function workspaceDirectories(manifest) {
  const directories = new Set([
    sandboxRoot,
    `${sandboxRoot}/build`,
    `${sandboxRoot}/generated`,
    `${sandboxRoot}/mocks`,
    `${sandboxRoot}/starter`,
    `${sandboxRoot}/tests`,
    `${sandboxRoot}/tests/public`,
    "/nonexistent",
  ]);
  for (const relativePath of [
    dirname(manifest.answer.output),
    manifest.paths.mocks,
    manifest.paths.publicTests,
    manifest.paths.starter,
  ]) {
    const segments = relativePath.split("/");
    let current = sandboxRoot;
    for (const segment of segments) {
      current = `${current}/${segment}`;
      directories.add(current);
    }
  }
  return [...directories]
    .sort((left, right) =>
      left.split("/").length - right.split("/").length);
}

export function buildSandboxInvocation({
  bubblewrapPath,
  prlimitPath,
  fixtureRoot,
  manifest,
  buildRoot,
  command,
  toolPath = null,
  systemMounts = existingSystemMounts(command.phase),
}) {
  if (!["compile", "test"].includes(command.phase)) {
    throw new TypeError(`unsupported sandbox phase: ${command.phase}`);
  }
  const limits = sandboxResourceLimits[command.phase];
  const sandboxArgs = [
    "--unshare-all",
    "--unshare-user",
    "--disable-userns",
    "--die-with-parent",
    "--new-session",
    "--cap-drop",
    "ALL",
    "--hostname",
    "benchmark-sandbox",
    "--clearenv",
    "--setenv",
    "HOME",
    "/nonexistent",
    "--setenv",
    "LANG",
    "C",
    "--setenv",
    "LC_ALL",
    "C",
    "--setenv",
    "PATH",
    "/usr/bin:/bin",
    "--setenv",
    "TMPDIR",
    "/tmp",
    "--size",
    String(rootTmpfsBytes),
    "--tmpfs",
    "/",
  ];
  for (const path of systemMounts) {
    sandboxArgs.push("--ro-bind", path, path);
  }
  sandboxArgs.push(
    "--proc",
    "/proc",
    "--dev",
    "/dev",
    "--size",
    String(temporaryDirectoryBytes),
    "--tmpfs",
    "/tmp",
  );
  for (const directory of workspaceDirectories(manifest)) {
    sandboxArgs.push("--dir", directory);
  }

  const readOnlyBindings = [
    [resolve(fixtureRoot, manifest.paths.starter),
      `${sandboxRoot}/${manifest.paths.starter}`],
    [resolve(fixtureRoot, manifest.paths.mocks),
      `${sandboxRoot}/${manifest.paths.mocks}`],
    [resolve(fixtureRoot, manifest.paths.publicTests),
      `${sandboxRoot}/${manifest.paths.publicTests}`],
    [resolve(fixtureRoot, manifest.answer.output),
      `${sandboxRoot}/${manifest.answer.output}`],
  ];
  for (const [source, destination] of readOnlyBindings) {
    sandboxArgs.push("--ro-bind", source, destination);
  }
  sandboxArgs.push(
    command.phase === "compile" ? "--bind" : "--ro-bind",
    buildRoot,
    `${sandboxRoot}/${manifest.paths.build}`,
    "--remount-ro",
    "/",
    "--chdir",
    sandboxRoot,
    "--",
  );

  const executable = command.argv[0].includes("/")
    ? command.argv[0]
    : toolPath;
  if (!executable) {
    throw new TypeError(`tool path is required for ${command.id}`);
  }
  sandboxArgs.push(executable, ...command.argv.slice(1));

  return {
    command: prlimitPath,
    args: [
      `--as=${limits.addressSpaceBytes}`,
      `--cpu=${limits.cpuSeconds}`,
      `--fsize=${limits.fileBytes}`,
      `--nofile=${limits.openFiles}`,
      "--core=0",
      "--",
      bubblewrapPath,
      ...sandboxArgs,
    ],
    options: {
      encoding: "utf8",
      killSignal: "SIGKILL",
      maxBuffer: maximumOutputBytes,
      stdio: ["ignore", "pipe", "pipe"],
      timeout: command.timeoutMs,
    },
  };
}

function phaseResult(command, result, startedAt, finishedAt) {
  return {
    id: command.id,
    phase: command.phase,
    argv: [...command.argv],
    timeoutMs: command.timeoutMs,
    startedAt: startedAt.toISOString(),
    finishedAt: finishedAt.toISOString(),
    durationMs: Math.max(0, finishedAt.getTime() - startedAt.getTime()),
    exitCode: Number.isInteger(result.status) ? result.status : null,
    signal: result.signal ?? null,
    timedOut: result.error?.code === "ETIMEDOUT",
    stdout: result.stdout ?? "",
    stderr: result.stderr ?? "",
    error: result.error
      ? `${result.error.code ?? "ERROR"}: ${result.error.message}`
      : null,
  };
}

function phaseSucceeded(phase) {
  return phase.exitCode === 0 &&
    phase.signal === null &&
    phase.error === null &&
    !phase.timedOut;
}

function validateTestExecutable(buildRoot, manifest, command) {
  const relativeExecutable = command.argv[0]
    .slice(`${manifest.paths.build}/`.length);
  const executablePath = resolve(buildRoot, relativeExecutable);
  requireContained(buildRoot, executablePath, "sandbox test executable");
  requireRegularFile(executablePath, "sandbox test executable");
  accessSync(executablePath, constants.X_OK);
}

function writeReport(path, report) {
  const directory = dirname(path);
  const fixtureRoot = dirname(directory);
  ensureSafeDirectory(fixtureRoot, directory);
  if (existsSync(path)) {
    const metadata = lstatSync(path);
    if (metadata.isSymbolicLink() || !metadata.isFile()) {
      throw new TypeError("validation report path is unsafe");
    }
  }
  const temporaryPath = join(
    directory,
    `.${basename(path)}.${randomUUID()}.tmp`,
  );
  try {
    writeFileSync(temporaryPath, `${JSON.stringify(report, null, 2)}\n`, {
      encoding: "utf8",
      flag: "wx",
      mode: 0o600,
    });
    renameSync(temporaryPath, path);
  } finally {
    rmSync(temporaryPath, { force: true });
  }
}

export function validateFixtureValidationReport(report) {
  const topLevelKeys = [
    "finishedAt",
    "fixtureStatus",
    "answerSha256",
    "phases",
    "sandbox",
    "schemaVersion",
    "startedAt",
    "success",
    "targetProfile",
    "taskId",
  ];
  requireExactKeys(report, topLevelKeys, "fixture validation report");
  if (report.schemaVersion !== "1.0") {
    throw new TypeError("unsupported fixture validation report version");
  }
  if (
    typeof report.taskId !== "string" ||
    !taskIdPattern.test(report.taskId)
  ) {
    throw new TypeError("fixture validation taskId is invalid");
  }
  if (
    typeof report.answerSha256 !== "string" ||
    !/^[a-f0-9]{64}$/u.test(report.answerSha256)
  ) {
    throw new TypeError("fixture validation answerSha256 is invalid");
  }
  if (
    report.targetProfile !== null &&
    !targetProfileSet.has(report.targetProfile)
  ) {
    throw new TypeError("fixture validation targetProfile is invalid");
  }
  if (!["active", "scaffold"].includes(report.fixtureStatus)) {
    throw new TypeError("fixture validation status is invalid");
  }
  requireDateTime(report.startedAt, "fixture validation startedAt");
  requireDateTime(report.finishedAt, "fixture validation finishedAt");
  if (Date.parse(report.finishedAt) < Date.parse(report.startedAt)) {
    throw new TypeError("fixture validation time range is invalid");
  }
  requireExactKeys(
    report.sandbox,
    ["filesystem", "network", "resourceLimits", "runtime"],
    "fixture validation sandbox",
  );
  if (
    report.sandbox.runtime !== "bubblewrap" ||
    report.sandbox.network !== "none" ||
    report.sandbox.filesystem !== "isolated"
  ) {
    throw new TypeError("fixture validation sandbox metadata is invalid");
  }
  requireExactKeys(
    report.sandbox.resourceLimits,
    ["compile", "test"],
    "fixture validation resource limits",
  );
  for (const phase of ["compile", "test"]) {
    const limits = report.sandbox.resourceLimits[phase];
    requireExactKeys(
      limits,
      [
        "addressSpaceBytes",
        "cpuSeconds",
        "fileBytes",
        "openFiles",
      ],
      `${phase} resource limits`,
    );
    for (const [name, value] of Object.entries(limits)) {
      requirePositiveInteger(value, `${phase}.${name}`);
    }
  }
  if (!Array.isArray(report.phases) || report.phases.length === 0) {
    throw new TypeError("fixture validation phases must be non-empty");
  }
  for (const phase of report.phases) {
    requireExactKeys(
      phase,
      [
        "argv",
        "durationMs",
        "error",
        "exitCode",
        "finishedAt",
        "id",
        "phase",
        "signal",
        "startedAt",
        "stderr",
        "stdout",
        "timedOut",
        "timeoutMs",
      ],
      "fixture validation phase",
    );
    if (
      typeof phase.id !== "string" ||
      !/^[a-z][a-z0-9-]*$/u.test(phase.id) ||
      !["compile", "test"].includes(phase.phase) ||
      !Array.isArray(phase.argv) ||
      phase.argv.length === 0 ||
      phase.argv.some((value) =>
        typeof value !== "string" || value.length === 0)
    ) {
      throw new TypeError("fixture validation phase metadata is invalid");
    }
    requirePositiveInteger(phase.timeoutMs, "phase timeoutMs");
    requireDateTime(phase.startedAt, "phase startedAt");
    requireDateTime(phase.finishedAt, "phase finishedAt");
    if (Date.parse(phase.finishedAt) < Date.parse(phase.startedAt)) {
      throw new TypeError("phase time range is invalid");
    }
    if (!Number.isSafeInteger(phase.durationMs) || phase.durationMs < 0) {
      throw new TypeError("phase durationMs is invalid");
    }
    if (
      (phase.exitCode !== null && !Number.isInteger(phase.exitCode)) ||
      (phase.signal !== null && typeof phase.signal !== "string") ||
      typeof phase.timedOut !== "boolean" ||
      typeof phase.stdout !== "string" ||
      typeof phase.stderr !== "string" ||
      (phase.error !== null && typeof phase.error !== "string")
    ) {
      throw new TypeError("fixture validation phase outcome is invalid");
    }
  }
  if (
    typeof report.success !== "boolean" ||
    report.success !== (
      report.phases.some((phase) => phase.phase === "compile") &&
      report.phases.some((phase) => phase.phase === "test") &&
      report.phases.every(phaseSucceeded)
    )
  ) {
    throw new TypeError("fixture validation success is inconsistent");
  }
  return report;
}

export function runFixtureValidation({
  taskId,
  fixturesRoot,
  tasksPath,
  spawn = spawnSync,
  now = () => new Date(),
  pathValue = process.env.PATH ?? "",
  resolveExecutableImpl = resolveExecutable,
}) {
  if (typeof taskId !== "string" || !taskIdPattern.test(taskId)) {
    throw new TypeError("taskId is invalid");
  }
  const root = resolve(asPath(fixturesRoot));
  requireDirectory(root, "fixtures root");
  const tasks = loadTasks(tasksPath);
  const task = tasks.find((item) => item.id === taskId);
  if (!task) throw new TypeError(`unknown fixture task: ${taskId}`);

  const fixtureRoot = resolve(root, taskId);
  requireContained(root, fixtureRoot, "fixture path");
  requireDirectory(fixtureRoot, `fixture ${taskId}`);
  const manifestPath = join(fixtureRoot, "manifest.json");
  requireRegularFile(manifestPath, `fixture ${taskId} manifest`);
  const manifest = validateFixtureManifest(
    JSON.parse(readFileSync(manifestPath, "utf8")),
    task,
  );
  const answerPath = resolve(fixtureRoot, manifest.answer.output);
  requireContained(fixtureRoot, answerPath, "fixture answer");
  requireRegularFile(answerPath, `fixture ${taskId} extracted answer`);
  const answerSha256 = createHash("sha256")
    .update(readFileSync(answerPath))
    .digest("hex");

  const bubblewrapPath = resolveExecutableImpl("bwrap", { pathValue });
  const prlimitPath = resolveExecutableImpl("prlimit", { pathValue });
  const toolPaths = new Map();
  for (const command of manifest.commands) {
    for (const tool of command.requiredTools) {
      toolPaths.set(tool, resolveExecutableImpl(tool, { pathValue }));
    }
  }

  const buildRoot = mkdtempSync(join(tmpdir(), `${taskId}-sandbox-build-`));
  const reportStartedAt = now();
  const phases = [];
  try {
    const commands = [
      ...manifest.commands.filter((command) => command.phase === "compile"),
      ...manifest.commands.filter((command) => command.phase === "test"),
    ];
    for (const command of commands) {
      if (phases.some((phase) => !phaseSucceeded(phase))) break;
      if (command.phase === "test") {
        const startedAt = now();
        try {
          validateTestExecutable(buildRoot, manifest, command);
        } catch (error) {
          const finishedAt = now();
          phases.push(phaseResult(command, {
            status: null,
            signal: null,
            stdout: "",
            stderr: "",
            error,
          }, startedAt, finishedAt));
          break;
        }
      }
      const invocation = buildSandboxInvocation({
        bubblewrapPath,
        prlimitPath,
        fixtureRoot,
        manifest,
        buildRoot,
        command,
        toolPath: command.argv[0].includes("/")
          ? null
          : toolPaths.get(command.argv[0]),
      });
      const startedAt = now();
      const result = spawn(
        invocation.command,
        invocation.args,
        invocation.options,
      );
      const finishedAt = now();
      phases.push(phaseResult(command, result, startedAt, finishedAt));
    }

    const report = {
      schemaVersion: "1.0",
      taskId,
      answerSha256,
      targetProfile: manifest.targetProfile,
      fixtureStatus: manifest.status,
      startedAt: reportStartedAt.toISOString(),
      finishedAt: now().toISOString(),
      sandbox: {
        runtime: "bubblewrap",
        network: "none",
        filesystem: "isolated",
        resourceLimits: sandboxResourceLimits,
      },
      phases,
      success: phases.some((phase) => phase.phase === "compile") &&
        phases.some((phase) => phase.phase === "test") &&
        phases.every(phaseSucceeded),
    };
    validateFixtureValidationReport(report);
    const reportPath = resolve(
      fixtureRoot,
      manifest.paths.build,
      "validation-report.json",
    );
    requireContained(fixtureRoot, reportPath, "validation report path");
    writeReport(reportPath, report);
    return { report, reportPath };
  } finally {
    rmSync(buildRoot, { recursive: true, force: true });
  }
}

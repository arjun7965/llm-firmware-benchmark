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
import { attestDependencyInstallation } from "./dependency-installation.mjs";
import { loadTasks } from "./harness.mjs";
import { requireSuite } from "./suites.mjs";
import { targetProfileSet } from "./target-profiles.mjs";
import {
  getValidationProfile,
  getValidationProfileRevision,
  profileFingerprint,
  requireValidationProfile,
  resolveValidationProfile,
  sandboxProfileBlockReason,
  selectValidationEnvironment,
  validateValidationEnvironmentReference,
  validationEnvironmentReference,
} from "./validation-profiles.mjs";

const taskIdPattern = /^[a-z0-9]+(?:-[a-z0-9]+)*$/;
const maximumOutputBytes = 1024 * 1024;
const sandboxRoot = "/workspace";
const versionProbeTimeoutMs = 5_000;
const architectureNames = Object.freeze({
  arm64: "aarch64",
  x64: "x86_64",
});

export const sandboxResourceLimits =
  getValidationProfile("c11-host").sandbox.resourceLimits;

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

function requireNonnegativeInteger(value, name) {
  if (!Number.isSafeInteger(value) || value < 0) {
    throw new TypeError(`${name} must be a nonnegative safe integer`);
  }
}

function decodeOsReleaseValue(value, name) {
  if (value === "") return "";
  const quote = value[0];
  if (quote !== "\"" && quote !== "'") {
    if (/\s/u.test(value)) {
      throw new TypeError(`${name} contains unquoted whitespace`);
    }
    return value;
  }
  if (value.length < 2 || value.at(-1) !== quote) {
    throw new TypeError(`${name} has an unterminated quote`);
  }
  const decoded = value.slice(1, -1);
  return quote === "\""
    ? decoded.replace(/\\(["\\$`])/gu, "$1")
    : decoded;
}

export function parseOsRelease(contents) {
  if (typeof contents !== "string" || contents.includes("\0")) {
    throw new TypeError("os-release contents are invalid");
  }
  const values = new Map();
  for (const rawLine of contents.split(/\r?\n/u)) {
    const line = rawLine.trim();
    if (line === "" || line.startsWith("#")) continue;
    const match = /^([A-Z][A-Z0-9_]*)=(.*)$/u.exec(line);
    if (!match) throw new TypeError("os-release contains an invalid line");
    const [, key, rawValue] = match;
    if (values.has(key)) {
      throw new TypeError(`os-release contains duplicate ${key}`);
    }
    values.set(key, decodeOsReleaseValue(
      rawValue,
      `os-release ${key}`,
    ));
  }
  return values;
}

export function readValidationHost({
  architecture = process.arch,
  osReleasePath = "/etc/os-release",
} = {}) {
  const osRelease = parseOsRelease(readFileSync(osReleasePath, "utf8"));
  const operatingSystem = osRelease.get("ID");
  const release = osRelease.get("VERSION_ID");
  const normalizedArchitecture = architectureNames[architecture] ??
    architecture;
  if (
    typeof operatingSystem !== "string" ||
    !/^[a-z0-9._-]+$/u.test(operatingSystem) ||
    typeof release !== "string" ||
    !/^[A-Za-z0-9._-]+$/u.test(release) ||
    typeof normalizedArchitecture !== "string" ||
    !/^[A-Za-z0-9._-]+$/u.test(normalizedArchitecture)
  ) {
    throw new TypeError("validation host metadata is invalid");
  }
  return {
    operatingSystem,
    release,
    architecture: normalizedArchitecture,
  };
}

function requireMatchingValidationHost(actual, expected) {
  requireExactKeys(
    actual,
    ["architecture", "operatingSystem", "release"],
    "validation host",
  );
  for (const field of ["operatingSystem", "release", "architecture"]) {
    if (actual[field] !== expected[field]) {
      throw new TypeError(
        `validation host does not match environment: expected ` +
        `${expected.operatingSystem} ${expected.release} ` +
        `${expected.architecture}`,
      );
    }
  }
  return actual;
}

function requireRunnableProfile(profile) {
  const blockReason = sandboxProfileBlockReason(profile);
  if (blockReason) {
    throw new TypeError(
      `validation profile ${profile.id}@${profile.revision} cannot run: ` +
      blockReason,
    );
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
  return metadata;
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

function versionMatches(version, expectedVersion) {
  const escapedVersion = expectedVersion.replace(
    /[.*+?^${}()|[\]\\]/gu,
    "\\$&",
  );
  return new RegExp(
    `(?:^|[^0-9.])${escapedVersion}(?![0-9A-Za-z._+~-])`,
    "u",
  ).test(version);
}

function inspectToolchain(
  name,
  executable,
  versionArgs,
  expectedVersion,
  spawnTool,
  {
    pathValue = "/usr/bin:/bin",
  } = {},
) {
  const result = spawnTool(executable, versionArgs, {
    encoding: "utf8",
    env: {
      LANG: "C",
      LC_ALL: "C",
      PATH: pathValue,
    },
    killSignal: "SIGKILL",
    maxBuffer: maximumOutputBytes,
    stdio: ["ignore", "pipe", "pipe"],
    timeout: versionProbeTimeoutMs,
  });
  if (
    result.error ||
    result.signal !== null ||
    result.status !== 0
  ) {
    throw new TypeError(`could not determine ${name} version`);
  }
  const version = `${result.stdout ?? ""}\n${result.stderr ?? ""}`
    .split(/\r?\n/u)
    .map((line) => line.trim())
    .find((line) => line !== "");
  if (!version || version.length > 1024 || version.includes("\0")) {
    throw new TypeError(`${name} returned an invalid version`);
  }
  if (!versionMatches(version, expectedVersion)) {
    throw new TypeError(
      `${name} version does not match validation profile ` +
      `(${expectedVersion} required)`,
    );
  }
  return {
    name,
    executable,
    version,
    versionArgv: [executable, ...versionArgs],
  };
}

function existingSystemMounts(phase, profile) {
  const optionalPaths = phase === "compile"
    ? ["/usr", "/bin", "/lib", "/lib64", "/etc/alternatives"]
    : ["/lib", "/lib64"];
  const requiredPaths = phase === "test"
    ? profile.testRuntime?.mounts.map((mount) => mount.path) ?? []
    : [];
  return [...new Set([
    ...optionalPaths.filter((path) => existsSync(path)),
    ...requiredPaths,
  ])];
}

function toolIsUnderRuntimeMount(toolPath, profile) {
  return profile.testRuntime?.mounts.some((mount) =>
    toolPath === mount.path || toolPath.startsWith(`${mount.path}/`)) ?? false;
}

function workspaceDirectories(manifest, profile) {
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
  const dependencyMountPath = profile.dependencyInstall?.mountPath;
  if (dependencyMountPath) {
    let current = "";
    for (const segment of dependencyMountPath.split("/").filter(Boolean)) {
      current += `/${segment}`;
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
  systemMounts = null,
}) {
  if (!["compile", "test"].includes(command.phase)) {
    throw new TypeError(`unsupported sandbox phase: ${command.phase}`);
  }
  const validationProfile = getValidationProfile(manifest.validationProfile);
  const sandbox = validationProfile.sandbox;
  const mountedSystemPaths = systemMounts ?? existingSystemMounts(
    command.phase,
    validationProfile,
  );
  const profileEnvironment = manifest.validationProfile === "go-std"
    ? [
      "--setenv",
      "GOCACHE",
      `${sandboxRoot}/build/go-cache`,
      "--setenv",
      "GOMODCACHE",
      `${sandboxRoot}/build/go-mod-cache`,
      "--setenv",
      "GOTOOLCHAIN",
      "local",
      "--setenv",
      "GOWORK",
      "off",
      "--setenv",
      "GOENV",
      "off",
      "--setenv",
      "CGO_ENABLED",
      "0",
      ...(command.phase === "compile" && toolPath
        ? [
          "--setenv",
          "FIXTURE_GO_EXECUTABLE",
          toolPath,
        ]
        : []),
    ]
    : manifest.validationProfile === "python3-stdlib" &&
        command.phase === "compile"
      ? [
        "--setenv",
        "PYTHONPYCACHEPREFIX",
        `${sandboxRoot}/build/pycache`,
      ]
      : [];
  const sandboxPath = manifest.validationProfile === "node-typescript"
    ? "/usr/local/bin:/usr/bin:/bin"
    : "/usr/bin:/bin";
  const limits = sandbox.resourceLimits[command.phase];
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
    sandboxPath,
    "--setenv",
    "TMPDIR",
    "/tmp",
    ...profileEnvironment,
    "--size",
    String(sandbox.rootTmpfsBytes),
    "--tmpfs",
    "/",
  ];
  for (const path of mountedSystemPaths) {
    sandboxArgs.push("--ro-bind", path, path);
  }
  sandboxArgs.push(
    "--proc",
    "/proc",
    "--dev",
    "/dev",
    "--size",
    String(sandbox.temporaryDirectoryBytes),
    "--tmpfs",
    "/tmp",
  );
  for (const directory of workspaceDirectories(manifest, validationProfile)) {
    sandboxArgs.push("--dir", directory);
  }

  const dependencyInstall = validationProfile.dependencyInstall;
  if (command.phase === "compile" && dependencyInstall?.installRoot) {
    sandboxArgs.push(
      "--ro-bind",
      dependencyInstall.installRoot,
      dependencyInstall.mountPath,
    );
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
  if (
    command.phase === "test" &&
    validationProfile.testRuntime &&
    !toolIsUnderRuntimeMount(executable, validationProfile)
  ) {
    throw new TypeError(
      `test executable for ${command.id} is outside its runtime mounts`,
    );
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

function resultOutcome(result) {
  if (result.error?.code === "ETIMEDOUT") return "timed-out";
  if (result.error) return "error";
  if (result.status === 0 && result.signal === null) return "passed";
  return "failed";
}

function phaseResult(command, result, startedAt, finishedAt) {
  return {
    id: command.id,
    phase: command.phase,
    argv: [...command.argv],
    outcome: resultOutcome(result),
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
  return phase.outcome === "passed";
}

function validateTestExecutable(buildRoot, manifest, command) {
  const relativeExecutable = command.argv[0]
    .slice(`${manifest.paths.build}/`.length);
  const executablePath = resolve(buildRoot, relativeExecutable);
  requireContained(buildRoot, executablePath, "sandbox test executable");
  const metadata = requireRegularFile(
    executablePath,
    "sandbox test executable",
  );
  accessSync(executablePath, constants.X_OK);
  return {
    path: command.argv[0],
    sizeBytes: metadata.size,
  };
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
    "artifacts",
    "finishedAt",
    "fixtureStatus",
    "answerSha256",
    "language",
    "phases",
    "sandbox",
    "schemaVersion",
    "suite",
    "startedAt",
    "success",
    "targetProfile",
    "taskId",
    "toolchains",
    "validationProfile",
    "validationProfileRevision",
    "validationProfileSha256",
    "validationEnvironment",
  ];
  requireExactKeys(report, topLevelKeys, "fixture validation report");
  if (report.schemaVersion !== "1.5") {
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
  requireSuite(report.suite, "fixture validation suite");
  requireValidationProfile(
    report.validationProfile,
    "fixture validation validationProfile",
  );
  const validationProfileContract = getValidationProfileRevision(
    report.validationProfile,
    report.validationProfileRevision,
    "fixture validation validationProfile",
  );
  if (
    report.validationProfileSha256 !==
      profileFingerprint(validationProfileContract)
  ) {
    throw new TypeError(
      "fixture validation validationProfileSha256 is invalid",
    );
  }
  requireExactKeys(
    report.validationEnvironment,
    ["execution", "host", "id", "revision", "sha256"],
    "fixture validation validationEnvironment",
  );
  const validationEnvironment = validateValidationEnvironmentReference(
    {
      id: report.validationEnvironment.id,
      revision: report.validationEnvironment.revision,
      sha256: report.validationEnvironment.sha256,
    },
    "fixture validation validationEnvironment",
  );
  requireMatchingValidationHost(
    report.validationEnvironment.host,
    validationEnvironment.host,
  );
  const expectedExecution = validationEnvironment.execution;
  requireExactKeys(
    report.validationEnvironment.execution,
    expectedExecution.kind === "oci" ? ["image", "kind"] : ["kind"],
    "fixture validation environment execution",
  );
  if (
    report.validationEnvironment.execution.kind !== expectedExecution.kind ||
    (
      expectedExecution.kind === "oci" &&
      report.validationEnvironment.execution.image !== expectedExecution.image
    )
  ) {
    throw new TypeError(
      "fixture validation environment execution is invalid",
    );
  }
  const validationProfile = resolveValidationProfile(
    validationProfileContract,
    validationEnvironment,
  );
  if (report.suite === "firmware" && report.targetProfile === null) {
    throw new TypeError(
      "firmware fixture validation requires a targetProfile",
    );
  }
  if (report.suite === "auxiliary" && report.targetProfile !== null) {
    throw new TypeError(
      "auxiliary fixture validation cannot have a targetProfile",
    );
  }
  if (!["active", "scaffold"].includes(report.fixtureStatus)) {
    throw new TypeError("fixture validation status is invalid");
  }
  if (
    typeof report.language !== "string" ||
    !/^[a-z][a-z0-9+._-]*$/u.test(report.language)
  ) {
    throw new TypeError("fixture validation language is invalid");
  }
  requireDateTime(report.startedAt, "fixture validation startedAt");
  requireDateTime(report.finishedAt, "fixture validation finishedAt");
  if (Date.parse(report.finishedAt) < Date.parse(report.startedAt)) {
    throw new TypeError("fixture validation time range is invalid");
  }
  requireExactKeys(
    report.sandbox,
    [
      "filesystem",
      "limiter",
      "limiterVersion",
      "network",
      "resourceLimits",
      "runtime",
      "runtimeVersion",
    ],
    "fixture validation sandbox",
  );
  if (
    report.sandbox.runtime !== validationProfile.sandbox.runtime ||
    report.sandbox.limiter !== validationProfile.sandbox.limiter ||
    report.sandbox.network !== validationProfile.sandbox.network ||
    report.sandbox.filesystem !== validationProfile.sandbox.filesystem
  ) {
    throw new TypeError("fixture validation sandbox metadata is invalid");
  }
  if (
    typeof report.sandbox.runtimeVersion !== "string" ||
    report.sandbox.runtimeVersion.length === 0 ||
    report.sandbox.runtimeVersion.length > 1024 ||
    report.sandbox.runtimeVersion.includes("\0") ||
    typeof report.sandbox.limiterVersion !== "string" ||
    report.sandbox.limiterVersion.length === 0 ||
    report.sandbox.limiterVersion.length > 1024 ||
    report.sandbox.limiterVersion.includes("\0")
  ) {
    throw new TypeError(
      "fixture validation sandbox versions are invalid",
    );
  }
  if (
    !versionMatches(
      report.sandbox.runtimeVersion,
      validationProfile.sandbox.runtimeVersion,
    ) ||
    !versionMatches(
      report.sandbox.limiterVersion,
      validationProfile.sandbox.limiterVersion,
    )
  ) {
    throw new TypeError(
      "fixture validation sandbox versions do not match profile",
    );
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
      if (
        value !==
          validationProfile.sandbox.resourceLimits[phase][name]
      ) {
        throw new TypeError(
          `fixture validation ${phase}.${name} does not match profile`,
        );
      }
    }
  }
  if (!Array.isArray(report.toolchains) || report.toolchains.length === 0) {
    throw new TypeError("fixture validation toolchains must be non-empty");
  }
  const toolchainNames = new Set();
  for (const toolchain of report.toolchains) {
    requireExactKeys(
      toolchain,
      ["executable", "name", "version", "versionArgv"],
      "fixture validation toolchain",
    );
    if (
      typeof toolchain.name !== "string" ||
      !/^[a-z0-9][a-z0-9+._-]*$/u.test(toolchain.name) ||
      toolchainNames.has(toolchain.name) ||
      typeof toolchain.executable !== "string" ||
      !toolchain.executable.startsWith("/usr/") ||
      typeof toolchain.version !== "string" ||
      toolchain.version.trim() === "" ||
      toolchain.version.length > 1024 ||
      toolchain.version.includes("\0") ||
      !Array.isArray(toolchain.versionArgv) ||
      toolchain.versionArgv.length < 2 ||
      toolchain.versionArgv[0] !== toolchain.executable ||
      toolchain.versionArgv.slice(1).some((arg) =>
        typeof arg !== "string" ||
        arg.length === 0 ||
        arg.includes("\0"))
    ) {
      throw new TypeError("fixture validation toolchain is invalid");
    }
    toolchainNames.add(toolchain.name);
    const expectedToolchain = validationProfile.toolchains
      .find((item) => item.name === toolchain.name);
    if (
      !expectedToolchain ||
      !versionMatches(toolchain.version, expectedToolchain.version)
    ) {
      throw new TypeError(
        "fixture validation toolchain does not match profile",
      );
    }
    const expectedVersionArgv = [
      toolchain.executable,
      ...expectedToolchain.versionArgs,
    ];
    if (
      toolchain.versionArgv.length !== expectedVersionArgv.length ||
      toolchain.versionArgv.some((argument, index) =>
        argument !== expectedVersionArgv[index])
    ) {
      throw new TypeError(
        "fixture validation toolchain versionArgv does not match profile",
      );
    }
  }
  const expectedToolchainNames = validationProfile.toolchains
    .map((toolchain) => toolchain.name);
  if (
    toolchainNames.size !== expectedToolchainNames.length ||
    expectedToolchainNames.some((name) => !toolchainNames.has(name))
  ) {
    throw new TypeError(
      "fixture validation toolchains do not cover profile exactly",
    );
  }
  if (!Array.isArray(report.artifacts)) {
    throw new TypeError("fixture validation artifacts must be an array");
  }
  const artifactPaths = new Set();
  for (const artifact of report.artifacts) {
    requireExactKeys(
      artifact,
      ["path", "sizeBytes"],
      "fixture validation artifact",
    );
    if (
      typeof artifact.path !== "string" ||
      !/^build\/[A-Za-z0-9._/-]+$/u.test(artifact.path) ||
      artifact.path.split("/").some((segment) =>
        segment === "" || segment === "." || segment === "..") ||
      artifactPaths.has(artifact.path)
    ) {
      throw new TypeError("fixture validation artifact path is invalid");
    }
    requireNonnegativeInteger(
      artifact.sizeBytes,
      "fixture validation artifact sizeBytes",
    );
    artifactPaths.add(artifact.path);
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
        "outcome",
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
      !["error", "failed", "passed", "timed-out"].includes(phase.outcome) ||
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
    const expectedOutcome = phase.timedOut
      ? "timed-out"
      : phase.error !== null
        ? "error"
        : phase.exitCode === 0 && phase.signal === null
          ? "passed"
          : "failed";
    if (phase.outcome !== expectedOutcome) {
      throw new TypeError(
        "fixture validation phase outcome is inconsistent",
      );
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
  if (
    report.success &&
    report.artifacts.length === 0 &&
    !validationProfileContract.testRuntime
  ) {
    throw new TypeError("successful validation must record an artifact");
  }
  return report;
}

export function runFixtureValidation({
  taskId,
  fixturesRoot,
  tasksPath,
  attestDependencyInstallationImpl = attestDependencyInstallation,
  spawn = spawnSync,
  spawnTool = spawnSync,
  now = () => new Date(),
  pathValue = process.env.PATH ?? "",
  resolveExecutableImpl = resolveExecutable,
  readValidationHostImpl = readValidationHost,
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
  const validationProfileContract = getValidationProfile(
    manifest.validationProfile,
  );
  requireRunnableProfile(validationProfileContract);
  if (validationProfileContract.dependencies.length > 0) {
    attestDependencyInstallationImpl(validationProfileContract);
  }
  const validationHost = readValidationHostImpl();
  const validationEnvironment = selectValidationEnvironment(
    validationProfileContract,
    validationHost,
  );
  const validationProfile = resolveValidationProfile(
    validationProfileContract,
    validationEnvironment,
  );
  const answerPath = resolve(fixtureRoot, manifest.answer.output);
  requireContained(fixtureRoot, answerPath, "fixture answer");
  requireRegularFile(answerPath, `fixture ${taskId} extracted answer`);
  const answerSha256 = createHash("sha256")
    .update(readFileSync(answerPath))
    .digest("hex");

  const bubblewrapPath = resolveExecutableImpl(
    validationEnvironment.sandbox.runtime.executable,
    { pathValue },
  );
  const prlimitPath = resolveExecutableImpl(
    validationEnvironment.sandbox.limiter.executable,
    { pathValue },
  );
  const sandboxRuntime = inspectToolchain(
    validationEnvironment.sandbox.runtime.executable,
    bubblewrapPath,
    validationEnvironment.sandbox.runtime.versionArgs,
    validationEnvironment.sandbox.runtime.version,
    spawnTool,
  );
  const sandboxLimiter = inspectToolchain(
    validationEnvironment.sandbox.limiter.executable,
    prlimitPath,
    validationEnvironment.sandbox.limiter.versionArgs,
    validationEnvironment.sandbox.limiter.version,
    spawnTool,
  );
  const profileToolchains = new Map(
    validationProfile.toolchains.map((toolchain) => [
      toolchain.name,
      toolchain,
    ]),
  );
  const toolPaths = new Map();
  for (const command of manifest.commands) {
    for (const tool of command.requiredTools) {
      toolPaths.set(tool, resolveExecutableImpl(tool, { pathValue }));
    }
  }
  const toolchains = [...toolPaths.entries()]
    .sort(([left], [right]) =>
      left < right ? -1 : left > right ? 1 : 0)
    .map(([name, executable]) => {
      const interpreterPath = name === "tsc" && toolPaths.has("node")
        ? [dirname(toolPaths.get("node")), "/usr/bin", "/bin"]
          .join(delimiter)
        : undefined;
      return inspectToolchain(
        name,
        executable,
        manifest.toolVersionArgs[name],
        profileToolchains.get(name).version,
        spawnTool,
        { pathValue: interpreterPath },
      );
    });

  const buildRoot = mkdtempSync(join(tmpdir(), `${taskId}-sandbox-build-`));
  const reportStartedAt = now();
  const phases = [];
  const artifacts = [];
  try {
    const commands = [
      ...manifest.commands.filter((command) => command.phase === "compile"),
      ...manifest.commands.filter((command) => command.phase === "test"),
    ];
    for (const command of commands) {
      if (phases.some((phase) => !phaseSucceeded(phase))) break;
      if (
        command.phase === "test" &&
        command.argv[0].startsWith(`${manifest.paths.build}/`)
      ) {
        const startedAt = now();
        try {
          const artifact = validateTestExecutable(
            buildRoot,
            manifest,
            command,
          );
          if (!artifacts.some((item) => item.path === artifact.path)) {
            artifacts.push(artifact);
          }
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
      schemaVersion: "1.5",
      taskId,
      answerSha256,
      suite: task.suite,
      targetProfile: manifest.targetProfile,
      validationProfile: manifest.validationProfile,
      validationProfileRevision: validationProfileContract.revision,
      validationProfileSha256: profileFingerprint(
        validationProfileContract,
      ),
      validationEnvironment: {
        ...validationEnvironmentReference(validationEnvironment),
        host: validationHost,
        execution: validationEnvironment.execution,
      },
      fixtureStatus: manifest.status,
      language: manifest.language,
      startedAt: reportStartedAt.toISOString(),
      finishedAt: now().toISOString(),
      sandbox: {
        runtime: "bubblewrap",
        runtimeVersion: sandboxRuntime.version,
        limiter: "prlimit",
        limiterVersion: sandboxLimiter.version,
        network: "none",
        filesystem: "isolated",
        resourceLimits: validationProfile.sandbox.resourceLimits,
      },
      toolchains,
      artifacts,
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

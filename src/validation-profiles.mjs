import { createHash } from "node:crypto";
import {
  existsSync,
  lstatSync,
  readFileSync,
} from "node:fs";
import {
  dirname,
  relative,
  resolve,
  sep,
} from "node:path";
import { fileURLToPath } from "node:url";
export {
  attestDependencyInstallation,
  dependencyInstallationFingerprint,
} from "./dependency-installation.mjs";

const identifierPattern = /^[a-z][a-z0-9-]*$/u;
const packageNamePattern = /^(?:@[a-z0-9._-]+\/)?[a-z0-9][a-z0-9._-]*$/u;
const toolNamePattern = /^[a-z0-9][a-z0-9+._-]*$/u;
const runtimeMountPattern =
  /^\/(?:usr|lib|lib64)(?:\/[A-Za-z0-9+._-]+)+$/u;
const versionPattern =
  /^\d+\.\d+(?:\.\d+)?(?:[-+][A-Za-z0-9.-]+)?$/u;
const imagePattern =
  /^[a-z0-9./_-]+(?::[A-Za-z0-9._-]+)?@sha256:[a-f0-9]{64}$/u;
const fingerprintPattern = /^[a-f0-9]{64}$/u;
const lockfilePattern = /^validation-locks\/[A-Za-z0-9._/-]+$/u;
const dependencyInstallMountPattern = /^\/workspace\/[A-Za-z0-9._-]+$/u;
const resourceLimitFields = [
  "addressSpaceBytes",
  "cpuSeconds",
  "fileBytes",
  "openFiles",
];
const phaseNames = ["compile", "test"];
const commandPhases = new Set(["analyze", "compile", "test"]);
const forbiddenCommandTools = new Set([
  "bash",
  "cmd",
  "powershell",
  "pwsh",
  "sh",
  "zsh",
]);

function requireObject(value, name) {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    throw new TypeError(`${name} must be an object`);
  }
}

function asPath(value) {
  return value instanceof URL ? fileURLToPath(value) : value;
}

function requireExactFields(value, fields, name) {
  requireObject(value, name);
  if (Object.keys(value).sort().join(",") !== [...fields].sort().join(",")) {
    throw new TypeError(`${name} has unexpected fields`);
  }
}

function requireString(value, name, pattern) {
  if (typeof value !== "string" || !pattern.test(value)) {
    throw new TypeError(`${name} is invalid`);
  }
}

function requireSafeRelativePath(value, name, pattern) {
  requireString(value, name, pattern);
  if (
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
    throw new TypeError(`${name} escapes its root`);
  }
}

function requirePositiveInteger(value, name) {
  if (!Number.isSafeInteger(value) || value < 1) {
    throw new TypeError(`${name} must be a positive safe integer`);
  }
}

function requireSortedUnique(values, name, key) {
  const identifiers = values.map(key);
  if (
    new Set(identifiers).size !== identifiers.length ||
    identifiers.join(",") !== [...identifiers].sort().join(",")
  ) {
    throw new TypeError(`${name} must be sorted and unique`);
  }
}

function validateRevisionHistory(values, name) {
  let previous = null;
  for (const value of values) {
    if (
      previous &&
      (
        value.id < previous.id ||
        (
          value.id === previous.id &&
          value.revision !== previous.revision + 1
        )
      )
    ) {
      throw new TypeError(`${name} revisions must be sorted and contiguous`);
    }
    if ((!previous || value.id !== previous.id) && value.revision !== 1) {
      throw new TypeError(`${name} ${value.id} revisions must start at 1`);
    }
    previous = value;
  }
}

function validateVersionArgs(versionArgs, name) {
  if (
    !Array.isArray(versionArgs) ||
    versionArgs.length === 0 ||
    versionArgs.some((argument) =>
      typeof argument !== "string" ||
      argument.length === 0 ||
      argument.includes("\0"))
  ) {
    throw new TypeError(`${name} versionArgs is invalid`);
  }
}

function validateToolchains(toolchains, name) {
  if (!Array.isArray(toolchains) || toolchains.length === 0) {
    throw new TypeError(`${name} toolchains must be non-empty`);
  }
  for (const toolchain of toolchains) {
    requireExactFields(
      toolchain,
      ["name", "version", "versionArgs"],
      `${name} toolchain`,
    );
    requireString(toolchain.name, `${name} toolchain name`, toolNamePattern);
    requireString(
      toolchain.version,
      `${name} toolchain version`,
      versionPattern,
    );
    validateVersionArgs(toolchain.versionArgs, name);
  }
  requireSortedUnique(
    toolchains,
    `${name} toolchains`,
    (toolchain) => toolchain.name,
  );
}

function validateDependencies(dependencies, profileId) {
  if (!Array.isArray(dependencies)) {
    throw new TypeError(
      `validation profile ${profileId} dependencies must be an array`,
    );
  }
  for (const dependency of dependencies) {
    requireExactFields(
      dependency,
      ["name", "source", "version"],
      `validation profile ${profileId} dependency`,
    );
    requireString(
      dependency.name,
      `validation profile ${profileId} dependency name`,
      packageNamePattern,
    );
    if (!["npm", "pypi"].includes(dependency.source)) {
      throw new TypeError(
        `validation profile ${profileId} dependency source is invalid`,
      );
    }
    requireString(
      dependency.version,
      `validation profile ${profileId} dependency version`,
      versionPattern,
    );
  }
  requireSortedUnique(
    dependencies,
    `validation profile ${profileId} dependencies`,
    (dependency) => `${dependency.source}:${dependency.name}`,
  );
}

function validateDependencyInstall(profile, environmentRevisionMap) {
  if (!Object.hasOwn(profile, "dependencyInstall")) return;
  if (profile.dependencies.length === 0) {
    throw new TypeError(
      `validation profile ${profile.id} dependencyInstall is unnecessary`,
    );
  }
  const install = profile.dependencyInstall;
  requireObject(
    install,
    `validation profile ${profile.id} dependencyInstall`,
  );
  if (install.kind === "lockfile") {
    const runtimeAttested = Object.hasOwn(install, "installRoot");
    requireExactFields(
      install,
      runtimeAttested
        ? [
          "installRoot",
          "installSha256",
          "kind",
          "lockfile",
          "mountPath",
          "sha256",
          "source",
        ]
        : ["kind", "lockfile", "sha256", "source"],
      `validation profile ${profile.id} dependencyInstall`,
    );
    requireString(
      install.source,
      `validation profile ${profile.id} dependencyInstall source`,
      /^(?:npm|pypi)$/u,
    );
    if (profile.dependencies.some((dependency) =>
      dependency.source !== install.source)) {
      throw new TypeError(
        `validation profile ${profile.id} dependencyInstall source ` +
        "does not cover its dependencies",
      );
    }
    requireSafeRelativePath(
      install.lockfile,
      `validation profile ${profile.id} dependencyInstall lockfile`,
      lockfilePattern,
    );
    requireString(
      install.sha256,
      `validation profile ${profile.id} dependencyInstall sha256`,
      fingerprintPattern,
    );
    if (runtimeAttested) {
      requireString(
        install.installRoot,
        `validation profile ${profile.id} dependencyInstall installRoot`,
        runtimeMountPattern,
      );
      requireString(
        install.installSha256,
        `validation profile ${profile.id} dependencyInstall installSha256`,
        fingerprintPattern,
      );
      requireString(
        install.mountPath,
        `validation profile ${profile.id} dependencyInstall mountPath`,
        dependencyInstallMountPattern,
      );
    }
    return;
  }
  if (install.kind === "oci-image") {
    requireExactFields(
      install,
      ["image", "kind"],
      `validation profile ${profile.id} dependencyInstall`,
    );
    requireString(
      install.image,
      `validation profile ${profile.id} dependencyInstall image`,
      imagePattern,
    );
    if (!Array.isArray(profile.environments)) {
      throw new TypeError(
        `validation profile ${profile.id} dependencyInstall image requires ` +
        "logical environments",
      );
    }
    for (const reference of profile.environments) {
      const environment = environmentRevisionMap.get(
        `${reference.id}@${reference.revision}`,
      );
      if (
        environment.execution.kind !== "oci" ||
        environment.execution.image !== install.image
      ) {
        throw new TypeError(
          `validation profile ${profile.id} dependencyInstall image ` +
          "does not match its environments",
        );
      }
    }
    return;
  }
  throw new TypeError(
    `validation profile ${profile.id} dependencyInstall kind is invalid`,
  );
}

function profileToolNames(profile) {
  return profile.toolchains.map((toolchain) =>
    typeof toolchain === "string" ? toolchain : toolchain.name);
}

function requireToolSubset(tools, profile, name) {
  if (
    !Array.isArray(tools) ||
    tools.length === 0 ||
    tools.some((tool) =>
      typeof tool !== "string" || !toolNamePattern.test(tool))
  ) {
    throw new TypeError(`${name} requiredTools are invalid`);
  }
  requireSortedUnique(tools, `${name} requiredTools`, (tool) => tool);
  const availableTools = profileToolNames(profile);
  for (const tool of tools) {
    if (!availableTools.includes(tool)) {
      throw new TypeError(`${name} tool ${tool} is not in its profile`);
    }
  }
}

function validateTestRuntime(profile) {
  if (!Object.hasOwn(profile, "testRuntime")) return;
  const name = `validation profile ${profile.id} testRuntime`;
  const fields = ["commandContracts", "mounts"];
  if (Object.hasOwn(profile.testRuntime, "service")) {
    fields.push("service");
  }
  requireExactFields(
    profile.testRuntime,
    fields,
    name,
  );
  if (
    !Array.isArray(profile.testRuntime.mounts) ||
    profile.testRuntime.mounts.length === 0
  ) {
    throw new TypeError(`${name} mounts must be non-empty`);
  }
  for (const mount of profile.testRuntime.mounts) {
    requireExactFields(mount, ["access", "path"], `${name} mount`);
    requireString(mount.path, `${name} mount path`, runtimeMountPattern);
    if (mount.access !== "read-only") {
      throw new TypeError(`${name} mount access is invalid`);
    }
  }
  requireSortedUnique(
    profile.testRuntime.mounts,
    `${name} mounts`,
    (mount) => mount.path,
  );
  if (
    !Array.isArray(profile.testRuntime.commandContracts) ||
    profile.testRuntime.commandContracts.length === 0
  ) {
    throw new TypeError(`${name} commandContracts must be non-empty`);
  }
  for (const contract of profile.testRuntime.commandContracts) {
    requireExactFields(
      contract,
      ["argvPrefix", "id", "phase", "requiredTools"],
      `${name} commandContract`,
    );
    requireString(
      contract.id,
      `${name} commandContract id`,
      identifierPattern,
    );
    if (!commandPhases.has(contract.phase)) {
      throw new TypeError(`${name} commandContract phase is invalid`);
    }
    if (
      !Array.isArray(contract.argvPrefix) ||
      contract.argvPrefix.length === 0 ||
      contract.argvPrefix.some((argument) =>
        typeof argument !== "string" ||
        argument.length === 0 ||
        argument.includes("\0"))
    ) {
      throw new TypeError(`${name} commandContract argvPrefix is invalid`);
    }
    if (
      !toolNamePattern.test(contract.argvPrefix[0]) ||
      forbiddenCommandTools.has(contract.argvPrefix[0])
    ) {
      throw new TypeError(`${name} commandContract executable is invalid`);
    }
    requireToolSubset(
      contract.requiredTools,
      profile,
      `${name} commandContract ${contract.id}`,
    );
    if (!contract.requiredTools.includes(contract.argvPrefix[0])) {
      throw new TypeError(
        `${name} commandContract ${contract.id} must invoke a required tool`,
      );
    }
  }
  requireSortedUnique(
    profile.testRuntime.commandContracts,
    `${name} commandContracts`,
    (contract) => `${contract.phase}:${contract.id}`,
  );
  if (Object.hasOwn(profile.testRuntime, "service")) {
    const service = profile.testRuntime.service;
    requireExactFields(
      service,
      [
        "initializeArgv",
        "kind",
        "readyArgv",
        "shutdownTimeoutMs",
        "startArgv",
        "startupTimeoutMs",
        "stopArgv",
      ],
      `${name} service`,
    );
    if (service.kind !== "postgresql") {
      throw new TypeError(`${name} service kind is invalid`);
    }
    requirePositiveInteger(
      service.startupTimeoutMs,
      `${name} service startupTimeoutMs`,
    );
    requirePositiveInteger(
      service.shutdownTimeoutMs,
      `${name} service shutdownTimeoutMs`,
    );
    const expectedExecutables = new Map([
      ["initializeArgv", "initdb"],
      ["readyArgv", "psql"],
      ["startArgv", "postgres"],
      ["stopArgv", "pg_ctl"],
    ]);
    const availableTools = profileToolNames(profile);
    for (const [field, executable] of expectedExecutables) {
      const argv = service[field];
      if (
        !Array.isArray(argv) ||
        argv.length === 0 ||
        argv.some((argument) =>
          typeof argument !== "string" ||
          argument.length === 0 ||
          argument.includes("\0")) ||
        argv[0] !== executable ||
        !availableTools.includes(executable)
      ) {
        throw new TypeError(`${name} service ${field} is invalid`);
      }
    }
  }
}

function validateResourcePolicy(sandbox, profileId, {
  legacy,
} = {}) {
  const fields = [
    "filesystem",
    "limiter",
    "network",
    "resourceLimits",
    "rootTmpfsBytes",
    "runtime",
    "temporaryDirectoryBytes",
  ];
  if (legacy) fields.push("limiterVersion", "runtimeVersion");
  requireExactFields(
    sandbox,
    fields,
    `validation profile ${profileId} sandbox`,
  );
  if (
    sandbox.runtime !== "bubblewrap" ||
    sandbox.limiter !== "prlimit" ||
    sandbox.network !== "none" ||
    sandbox.filesystem !== "isolated"
  ) {
    throw new TypeError(
      `validation profile ${profileId} sandbox policy is invalid`,
    );
  }
  if (legacy) {
    requireString(
      sandbox.runtimeVersion,
      `validation profile ${profileId} sandbox runtimeVersion`,
      versionPattern,
    );
    requireString(
      sandbox.limiterVersion,
      `validation profile ${profileId} sandbox limiterVersion`,
      versionPattern,
    );
  }
  requirePositiveInteger(
    sandbox.rootTmpfsBytes,
    `validation profile ${profileId} sandbox rootTmpfsBytes`,
  );
  requirePositiveInteger(
    sandbox.temporaryDirectoryBytes,
    `validation profile ${profileId} sandbox temporaryDirectoryBytes`,
  );
  requireExactFields(
    sandbox.resourceLimits,
    phaseNames,
    `validation profile ${profileId} resourceLimits`,
  );
  for (const phase of phaseNames) {
    const limits = sandbox.resourceLimits[phase];
    requireExactFields(
      limits,
      resourceLimitFields,
      `validation profile ${profileId} ${phase} limits`,
    );
    for (const [limitName, value] of Object.entries(limits)) {
      requirePositiveInteger(
        value,
        `validation profile ${profileId} ${phase}.${limitName}`,
      );
    }
  }
}

function validateHost(host, name) {
  requireExactFields(
    host,
    ["architecture", "operatingSystem", "release"],
    name,
  );
  requireString(
    host.operatingSystem,
    `${name} operatingSystem`,
    /^[a-z0-9._-]+$/u,
  );
  requireString(host.release, `${name} release`, /^[A-Za-z0-9._-]+$/u);
  requireString(
    host.architecture,
    `${name} architecture`,
    /^[A-Za-z0-9._-]+$/u,
  );
}

function validateSandboxTool(tool, role, environmentId) {
  requireExactFields(
    tool,
    ["executable", "name", "version", "versionArgs"],
    `validation environment ${environmentId} ${role}`,
  );
  const expected = role === "runtime"
    ? { executable: "bwrap", name: "bubblewrap" }
    : { executable: "prlimit", name: "prlimit" };
  if (tool.executable !== expected.executable || tool.name !== expected.name) {
    throw new TypeError(
      `validation environment ${environmentId} ${role} is invalid`,
    );
  }
  requireString(
    tool.version,
    `validation environment ${environmentId} ${role} version`,
    versionPattern,
  );
  validateVersionArgs(
    tool.versionArgs,
    `validation environment ${environmentId} ${role}`,
  );
}

function validateEnvironment(environment) {
  requireExactFields(
    environment,
    [
      "execution",
      "host",
      "id",
      "revision",
      "sandbox",
      "toolchains",
    ],
    "validation environment",
  );
  requireString(
    environment.id,
    "validation environment id",
    identifierPattern,
  );
  requirePositiveInteger(
    environment.revision,
    `validation environment ${environment.id} revision`,
  );
  validateHost(
    environment.host,
    `validation environment ${environment.id} host`,
  );
  requireObject(
    environment.execution,
    `validation environment ${environment.id} execution`,
  );
  if (environment.execution.kind === "host") {
    requireExactFields(
      environment.execution,
      ["kind"],
      `validation environment ${environment.id} execution`,
    );
  } else if (environment.execution.kind === "oci") {
    requireExactFields(
      environment.execution,
      ["image", "kind"],
      `validation environment ${environment.id} execution`,
    );
    requireString(
      environment.execution.image,
      `validation environment ${environment.id} image`,
      imagePattern,
    );
  } else {
    throw new TypeError(
      `validation environment ${environment.id} execution kind is invalid`,
    );
  }
  requireExactFields(
    environment.sandbox,
    ["limiter", "runtime"],
    `validation environment ${environment.id} sandbox`,
  );
  validateSandboxTool(
    environment.sandbox.runtime,
    "runtime",
    environment.id,
  );
  validateSandboxTool(
    environment.sandbox.limiter,
    "limiter",
    environment.id,
  );
  validateToolchains(
    environment.toolchains,
    `validation environment ${environment.id}`,
  );
}

function validateLegacyProfile(profile) {
  const fields = [
    "dependencies",
    "host",
    "id",
    "revision",
    "sandbox",
    "toolchains",
  ];
  if (Object.hasOwn(profile, "dependencyInstall")) {
    fields.push("dependencyInstall");
  }
  if (Object.hasOwn(profile, "testRuntime")) {
    fields.push("testRuntime");
  }
  requireExactFields(
    profile,
    fields,
    "validation profile",
  );
  validateHost(profile.host, `validation profile ${profile.id} host`);
  validateToolchains(
    profile.toolchains,
    `validation profile ${profile.id}`,
  );
  validateResourcePolicy(profile.sandbox, profile.id, { legacy: true });
}

function validateLogicalProfile(profile, environmentRevisionMap) {
  const fields = [
    "dependencies",
    "environments",
    "id",
    "revision",
    "sandbox",
    "toolchains",
  ];
  if (Object.hasOwn(profile, "dependencyInstall")) {
    fields.push("dependencyInstall");
  }
  if (Object.hasOwn(profile, "testRuntime")) {
    fields.push("testRuntime");
  }
  requireExactFields(
    profile,
    fields,
    "validation profile",
  );
  if (!Array.isArray(profile.environments) ||
      profile.environments.length === 0) {
    throw new TypeError(
      `validation profile ${profile.id} environments must be non-empty`,
    );
  }
  for (const reference of profile.environments) {
    requireExactFields(
      reference,
      ["id", "revision"],
      `validation profile ${profile.id} environment`,
    );
    requireString(
      reference.id,
      `validation profile ${profile.id} environment id`,
      identifierPattern,
    );
    requirePositiveInteger(
      reference.revision,
      `validation profile ${profile.id} environment revision`,
    );
    if (!environmentRevisionMap.has(
      `${reference.id}@${reference.revision}`,
    )) {
      throw new TypeError(
        `validation profile ${profile.id} environment is unknown`,
      );
    }
  }
  requireSortedUnique(
    profile.environments,
    `validation profile ${profile.id} environments`,
    (reference) => `${reference.id}@${reference.revision}`,
  );
  if (
    !Array.isArray(profile.toolchains) ||
    profile.toolchains.length === 0 ||
    profile.toolchains.some((name) =>
      typeof name !== "string" || !toolNamePattern.test(name))
  ) {
    throw new TypeError(
      `validation profile ${profile.id} toolchains are invalid`,
    );
  }
  requireSortedUnique(
    profile.toolchains,
    `validation profile ${profile.id} toolchains`,
    (name) => name,
  );
  for (const reference of profile.environments) {
    const environment = environmentRevisionMap.get(
      `${reference.id}@${reference.revision}`,
    );
    const available = environment.toolchains
      .map((toolchain) => toolchain.name);
    if (
      available.length !== profile.toolchains.length ||
      available.some((name, index) => name !== profile.toolchains[index])
    ) {
      throw new TypeError(
        `validation profile ${profile.id} toolchains must exactly match ` +
        `environment ${reference.id}@${reference.revision}`,
      );
    }
  }
  validateResourcePolicy(profile.sandbox, profile.id);
  validateTestRuntime(profile);
}

export function validateValidationProfiles(document) {
  requireExactFields(
    document,
    ["environments", "profiles", "schemaVersion"],
    "validation profiles document",
  );
  if (document.schemaVersion !== "2.5") {
    throw new TypeError("unsupported validation profiles schemaVersion");
  }
  if (
    !Array.isArray(document.environments) ||
    document.environments.length === 0
  ) {
    throw new TypeError("validation environments must be a non-empty array");
  }
  for (const environment of document.environments) {
    validateEnvironment(environment);
  }
  validateRevisionHistory(
    document.environments,
    "validation environment",
  );
  const environmentRevisionMap = new Map(
    document.environments.map((environment) => [
      `${environment.id}@${environment.revision}`,
      environment,
    ]),
  );
  if (!Array.isArray(document.profiles) || document.profiles.length === 0) {
    throw new TypeError("validation profiles must be a non-empty array");
  }
  for (const profile of document.profiles) {
    requireString(profile.id, "validation profile id", identifierPattern);
    requirePositiveInteger(
      profile.revision,
      `validation profile ${profile.id} revision`,
    );
    validateDependencies(profile.dependencies, profile.id);
    if (Object.hasOwn(profile, "host")) {
      validateLegacyProfile(profile);
    } else {
      validateLogicalProfile(profile, environmentRevisionMap);
    }
    if (Object.hasOwn(profile, "host")) validateTestRuntime(profile);
    validateDependencyInstall(profile, environmentRevisionMap);
  }
  validateRevisionHistory(document.profiles, "validation profile");
  const currentProfiles = new Map();
  for (const profile of document.profiles) {
    currentProfiles.set(profile.id, profile);
  }
  for (const profile of currentProfiles.values()) {
    if (Object.hasOwn(profile, "host")) {
      throw new TypeError(
        `current validation profile ${profile.id}@${profile.revision} ` +
        "must use the logical environment shape",
      );
    }
    if (
      profile.dependencies.length > 0 &&
      !Object.hasOwn(profile, "dependencyInstall")
    ) {
      throw new TypeError(
        `current validation profile ${profile.id}@${profile.revision} ` +
        "must define dependencyInstall",
      );
    }
  }
  return document;
}

function deepFreeze(value) {
  if (value && typeof value === "object" && !Object.isFrozen(value)) {
    for (const child of Object.values(value)) deepFreeze(child);
    Object.freeze(value);
  }
  return value;
}

function canonicalize(value) {
  if (Array.isArray(value)) return value.map(canonicalize);
  if (value && typeof value === "object") {
    return Object.fromEntries(
      Object.keys(value)
        .sort()
        .map((key) => [key, canonicalize(value[key])]),
    );
  }
  return value;
}

function contractFingerprint(contract) {
  return createHash("sha256")
    .update(JSON.stringify(canonicalize(contract)))
    .digest("hex");
}

export function profileFingerprint(profile) {
  return contractFingerprint(profile);
}

export function environmentFingerprint(environment) {
  return contractFingerprint(environment);
}

function validateFingerprintGroup(values, fingerprints, name, fingerprint) {
  const ids = [...new Set(values.map((value) => value.id))];
  requireExactFields(fingerprints, ids, `${name} fingerprints`);
  for (const id of ids) {
    const revisions = values
      .filter((value) => value.id === id)
      .map((value) => String(value.revision));
    const revisionFingerprints = fingerprints[id];
    requireExactFields(
      revisionFingerprints,
      revisions,
      `${name} ${id} fingerprints`,
    );
    for (const value of values.filter((item) => item.id === id)) {
      const expected = revisionFingerprints[String(value.revision)];
      if (
        typeof expected !== "string" ||
        !fingerprintPattern.test(expected) ||
        expected !== fingerprint(value)
      ) {
        throw new TypeError(
          `${name} ${id}@${value.revision} fingerprint is invalid`,
        );
      }
    }
  }
}

export function validateValidationProfileFingerprints(
  profilesDocument,
  fingerprintsDocument,
) {
  validateValidationProfiles(profilesDocument);
  requireExactFields(
    fingerprintsDocument,
    ["environments", "profiles", "schemaVersion"],
    "validation profile fingerprints document",
  );
  if (fingerprintsDocument.schemaVersion !== "2.5") {
    throw new TypeError(
      "unsupported validation profile fingerprints schemaVersion",
    );
  }
  validateFingerprintGroup(
    profilesDocument.environments,
    fingerprintsDocument.environments,
    "validation environment",
    environmentFingerprint,
  );
  validateFingerprintGroup(
    profilesDocument.profiles,
    fingerprintsDocument.profiles,
    "validation profile",
    profileFingerprint,
  );
  return fingerprintsDocument;
}

export function normalizeDependencyLockfileContent(content) {
  return content.toString("utf8").replace(/\r\n/gu, "\n");
}

function validateNpmPackageLock(lockfile, profile) {
  requireExactFields(
    lockfile,
    ["lockfileVersion", "name", "packages", "requires", "version"],
    `validation profile ${profile.id} npm dependency lockfile`,
  );
  if (
    lockfile.lockfileVersion !== 3 ||
    lockfile.requires !== true ||
    lockfile.name !== `${profile.id}-validation-profile` ||
    lockfile.version !== `${profile.revision}.0.0`
  ) {
    throw new TypeError(
      `validation profile ${profile.id} npm dependency lockfile metadata ` +
      "does not match",
    );
  }
  requireObject(
    lockfile.packages,
    `validation profile ${profile.id} npm dependency lockfile packages`,
  );
  const rootPackage = lockfile.packages[""];
  requireExactFields(
    rootPackage,
    ["devDependencies", "name", "version"],
    `validation profile ${profile.id} npm dependency lockfile root package`,
  );
  const expectedDependencies = Object.fromEntries(
    profile.dependencies.map((dependency) => [
      dependency.name,
      dependency.version,
    ]),
  );
  if (
    rootPackage.name !== lockfile.name ||
    rootPackage.version !== lockfile.version ||
    JSON.stringify(rootPackage.devDependencies) !==
      JSON.stringify(expectedDependencies)
  ) {
    throw new TypeError(
      `validation profile ${profile.id} npm dependency lockfile ` +
      "dependencies do not match",
    );
  }
  const packagePaths = Object.keys(lockfile.packages);
  if (
    packagePaths.length < profile.dependencies.length + 1 ||
    packagePaths.some((path, index) =>
      (index === 0 && path !== "") ||
      (index > 0 && !path.startsWith("node_modules/")))
  ) {
    throw new TypeError(
      `validation profile ${profile.id} npm dependency lockfile package ` +
      "paths are invalid",
    );
  }
  for (const path of packagePaths.slice(1)) {
    const installedPackage = lockfile.packages[path];
    requireObject(
      installedPackage,
      `validation profile ${profile.id} npm dependency lockfile package`,
    );
    requireString(
      installedPackage.version,
      `validation profile ${profile.id} npm dependency lockfile package version`,
      versionPattern,
    );
    requireString(
      installedPackage.resolved,
      `validation profile ${profile.id} npm dependency lockfile package resolved`,
      /^https:\/\/registry\.npmjs\.org\/[A-Za-z0-9@%+._/-]+\.tgz$/u,
    );
    requireString(
      installedPackage.integrity,
      `validation profile ${profile.id} npm dependency lockfile package integrity`,
      /^sha512-[A-Za-z0-9+/]+={0,2}$/u,
    );
  }
}

function validatePypiRequirementsLock(content, profile) {
  const dependencies = normalizeDependencyLockfileContent(content)
    .split("\n")
    .filter((line) => line !== "")
    .map((line) => {
      const match = line.match(
        /^([a-z0-9][a-z0-9._-]*)==([0-9][A-Za-z0-9.+-]*) --hash=sha256:([a-f0-9]{64})$/u,
      );
      if (!match) {
        throw new TypeError(
          `validation profile ${profile.id} PyPI dependency lockfile ` +
          "entry is invalid",
        );
      }
      return { name: match[1], version: match[2] };
    });
  requireSortedUnique(
    dependencies,
    `validation profile ${profile.id} PyPI dependency lockfile dependencies`,
    (dependency) => dependency.name,
  );
  const expected = profile.dependencies.map((dependency) => ({
    name: dependency.name,
    version: dependency.version,
  }));
  if (JSON.stringify(dependencies) !== JSON.stringify(expected)) {
    throw new TypeError(
      `validation profile ${profile.id} PyPI dependency lockfile ` +
      "dependencies do not match",
    );
  }
}

function lockfileFingerprint(content) {
  return createHash("sha256")
    .update(normalizeDependencyLockfileContent(content))
    .digest("hex");
}

function validateDependencyLockfile(lockfile, profile, source) {
  if (source === "npm" && lockfile.lockfileVersion === 3) {
    validateNpmPackageLock(lockfile, profile);
    return;
  }
  requireExactFields(
    lockfile,
    ["dependencies", "profile", "schemaVersion", "source"],
    `validation profile ${profile.id} dependency lockfile`,
  );
  if (lockfile.schemaVersion !== "1.0") {
    throw new TypeError(
      `validation profile ${profile.id} dependency lockfile schemaVersion ` +
      "is unsupported",
    );
  }
  requireExactFields(
    lockfile.profile,
    ["id", "revision"],
    `validation profile ${profile.id} dependency lockfile profile`,
  );
  if (
    lockfile.profile.id !== profile.id ||
    lockfile.profile.revision !== profile.revision
  ) {
    throw new TypeError(
      `validation profile ${profile.id} dependency lockfile profile ` +
      "does not match",
    );
  }
  if (lockfile.source !== source) {
    throw new TypeError(
      `validation profile ${profile.id} dependency lockfile source ` +
      "does not match",
    );
  }
  if (!Array.isArray(lockfile.dependencies)) {
    throw new TypeError(
      `validation profile ${profile.id} dependency lockfile dependencies ` +
      "must be an array",
    );
  }
  for (const dependency of lockfile.dependencies) {
    requireExactFields(
      dependency,
      ["name", "version"],
      `validation profile ${profile.id} dependency lockfile dependency`,
    );
    requireString(
      dependency.name,
      `validation profile ${profile.id} dependency lockfile dependency name`,
      packageNamePattern,
    );
    requireString(
      dependency.version,
      `validation profile ${profile.id} dependency lockfile dependency version`,
      versionPattern,
    );
  }
  requireSortedUnique(
    lockfile.dependencies,
    `validation profile ${profile.id} dependency lockfile dependencies`,
    (dependency) => dependency.name,
  );
  const expected = profile.dependencies.map((dependency) => ({
    name: dependency.name,
    version: dependency.version,
  }));
  if (JSON.stringify(lockfile.dependencies) !== JSON.stringify(expected)) {
    throw new TypeError(
      `validation profile ${profile.id} dependency lockfile dependencies ` +
      "do not match",
    );
  }
}

export function validateValidationProfileLockfiles(
  profilesDocument,
  root,
) {
  validateValidationProfiles(profilesDocument);
  const resolvedRoot = resolve(asPath(root));
  for (const profile of profilesDocument.profiles) {
    if (profile.dependencyInstall?.kind !== "lockfile") continue;
    const lockfilePath = resolve(
      resolvedRoot,
      profile.dependencyInstall.lockfile,
    );
    requireContained(
      resolvedRoot,
      lockfilePath,
      `validation profile ${profile.id} dependencyInstall lockfile`,
    );
    requireRegularFile(
      lockfilePath,
      `validation profile ${profile.id} dependencyInstall lockfile`,
    );
    const content = readFileSync(lockfilePath);
    if (lockfileFingerprint(content) !== profile.dependencyInstall.sha256) {
      throw new TypeError(
        `validation profile ${profile.id} dependencyInstall sha256 ` +
        "does not match",
      );
    }
    if (
      profile.dependencyInstall.source === "pypi" &&
      !normalizeDependencyLockfileContent(content).trimStart().startsWith("{")
    ) {
      validatePypiRequirementsLock(content, profile);
    } else {
      validateDependencyLockfile(
        JSON.parse(normalizeDependencyLockfileContent(content)),
        profile,
        profile.dependencyInstall.source,
      );
    }
  }
  return profilesDocument;
}

export function loadValidationProfiles(
  path = new URL("../validation-profiles.json", import.meta.url),
  fingerprintsPath = new URL(
    "../validation-profile-fingerprints.json",
    import.meta.url,
  ),
) {
  const resolvedPath = asPath(path);
  const document = JSON.parse(readFileSync(resolvedPath, "utf8"));
  const fingerprints = JSON.parse(readFileSync(fingerprintsPath, "utf8"));
  validateValidationProfileFingerprints(document, fingerprints);
  validateValidationProfileLockfiles(document, dirname(resolvedPath));
  return deepFreeze(document);
}

export const validationProfilesDocument = loadValidationProfiles();
export const validationProfiles = validationProfilesDocument.profiles;
export const validationEnvironments =
  validationProfilesDocument.environments;
export const validationProfileIds = Object.freeze(
  [...new Set(validationProfiles.map((profile) => profile.id))],
);
export const validationEnvironmentIds = Object.freeze(
  [...new Set(validationEnvironments.map((environment) => environment.id))],
);
export const validationProfileSet = new Set(validationProfileIds);
export const validationEnvironmentSet = new Set(validationEnvironmentIds);
export const sandboxRunnableValidationProfileIds = Object.freeze([
  "c11-host",
  "go-std",
  "node-typescript",
  "python3-pytest-hypothesis",
  "python3-stdlib",
  "postgresql",
  "react18-typescript",
  "stable-rust",
]);
const sandboxRunnableValidationProfileSet = new Set(
  sandboxRunnableValidationProfileIds,
);

function revisionMaps(values) {
  const current = new Map();
  const revisions = new Map();
  for (const value of values) {
    current.set(value.id, value);
    revisions.set(`${value.id}@${value.revision}`, value);
  }
  return { current, revisions };
}

const profileMaps = revisionMaps(validationProfiles);
const environmentMaps = revisionMaps(validationEnvironments);

export function requireValidationProfile(
  value,
  name = "validationProfile",
) {
  if (typeof value !== "string" || !validationProfileSet.has(value)) {
    throw new TypeError(`${name} is invalid`);
  }
  return value;
}

export function getValidationProfile(value, name = "validationProfile") {
  return profileMaps.current.get(requireValidationProfile(value, name));
}

export function getValidationProfileRevision(
  value,
  revision,
  name = "validationProfile",
) {
  requireValidationProfile(value, name);
  requirePositiveInteger(revision, `${name}Revision`);
  const profile = profileMaps.revisions.get(`${value}@${revision}`);
  if (!profile) throw new TypeError(`${name}Revision is unknown`);
  return profile;
}

export function getValidationEnvironmentRevision(
  value,
  revision,
  name = "validationEnvironment",
) {
  if (typeof value !== "string" || !validationEnvironmentSet.has(value)) {
    throw new TypeError(`${name} is invalid`);
  }
  requirePositiveInteger(revision, `${name}Revision`);
  const environment = environmentMaps.revisions.get(`${value}@${revision}`);
  if (!environment) throw new TypeError(`${name}Revision is unknown`);
  return environment;
}

function hostsMatch(left, right) {
  return ["operatingSystem", "release", "architecture"]
    .every((field) => left[field] === right[field]);
}

export function selectValidationEnvironment(profile, actualHost) {
  if (!Array.isArray(profile.environments)) {
    throw new TypeError(
      `validation profile ${profile.id}@${profile.revision} is legacy-only`,
    );
  }
  validateHost(actualHost, "validation host");
  const matches = profile.environments
    .map((reference) => getValidationEnvironmentRevision(
      reference.id,
      reference.revision,
    ))
    .filter((environment) =>
      environment.execution.kind === "host" &&
      hostsMatch(environment.host, actualHost));
  if (matches.length !== 1) {
    const supported = profile.environments
      .map((reference) => `${reference.id}@${reference.revision}`)
      .join(", ");
    throw new TypeError(
      `validation host does not match exactly one supported environment: ` +
      supported,
    );
  }
  return matches[0];
}

export function resolveValidationProfile(profile, environment) {
  if (
    !profile.environments?.some((reference) =>
      reference.id === environment.id &&
      reference.revision === environment.revision)
  ) {
    throw new TypeError(
      `validation environment is not supported by profile ` +
      `${profile.id}@${profile.revision}`,
    );
  }
  const tools = new Map(
    environment.toolchains.map((toolchain) => [toolchain.name, toolchain]),
  );
  return {
    ...profile,
    environment,
    toolchains: profile.toolchains.map((name) => tools.get(name)),
    sandbox: {
      ...profile.sandbox,
      runtimeVersion: environment.sandbox.runtime.version,
      limiterVersion: environment.sandbox.limiter.version,
    },
  };
}

export function validationEnvironmentReference(environment) {
  return {
    id: environment.id,
    revision: environment.revision,
    sha256: environmentFingerprint(environment),
  };
}

export function validationProfileReference(profile) {
  return {
    id: profile.id,
    revision: profile.revision,
    sha256: profileFingerprint(profile),
  };
}

function requiredToolsMatch(left, right) {
  const leftTools = [...left].sort();
  const rightTools = [...right].sort();
  return leftTools.length === rightTools.length &&
    leftTools.every((tool, index) => tool === rightTools[index]);
}

export function validationProfileCommandContract(profile, command) {
  if (!profile.testRuntime) return null;
  return profile.testRuntime.commandContracts.find((contract) =>
    contract.phase === command.phase &&
    requiredToolsMatch(command.requiredTools, contract.requiredTools) &&
    command.argv.length >= contract.argvPrefix.length &&
    contract.argvPrefix.every((argument, index) =>
      command.argv[index] === argument)) ?? null;
}

export function validateValidationProfileReference(
  reference,
  name = "validationProfile",
) {
  requireExactFields(reference, ["id", "revision", "sha256"], name);
  const profile = getValidationProfileRevision(
    reference.id,
    reference.revision,
    name,
  );
  if (
    typeof reference.sha256 !== "string" ||
    reference.sha256 !== profileFingerprint(profile)
  ) {
    throw new TypeError(`${name}Sha256 is invalid`);
  }
  return profile;
}

export function validateValidationEnvironmentReference(
  reference,
  name = "validationEnvironment",
) {
  requireExactFields(reference, ["id", "revision", "sha256"], name);
  const environment = getValidationEnvironmentRevision(
    reference.id,
    reference.revision,
    name,
  );
  if (
    typeof reference.sha256 !== "string" ||
    reference.sha256 !== environmentFingerprint(environment)
  ) {
    throw new TypeError(`${name}Sha256 is invalid`);
  }
  return environment;
}

export function sandboxProfileBlockReason(profile) {
  if (
    profile.dependencies.length > 0 &&
    (
      profile.dependencyInstall?.kind !== "lockfile" ||
      !Object.hasOwn(profile.dependencyInstall, "installRoot")
    )
  ) {
    return "its validation profile dependency installation cannot be runtime-attested";
  }
  if (!sandboxRunnableValidationProfileSet.has(profile.id)) {
    if (profile.testRuntime) {
      return "its validation profile test runtime is not mounted by the current runner";
    }
    return "its validation profile requires an unavailable test runtime";
  }
  return null;
}

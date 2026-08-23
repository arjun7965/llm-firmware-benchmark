import { createHash } from "node:crypto";
import { spawnSync } from "node:child_process";
import {
  existsSync,
  lstatSync,
  mkdirSync,
  readFileSync,
  readdirSync,
  realpathSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { resolve } from "node:path";

const maximumOutputBytes = 1024 * 1024;
const probeTimeoutMs = 5_000;
const ociProcessLimit = 256;
const ociUserId = 65_532;
const imagePattern =
  /^[a-z0-9](?:[a-z0-9._-]*[a-z0-9])?(?:\/[a-z0-9](?:[a-z0-9._-]*[a-z0-9])?)*(?::[A-Za-z0-9_][A-Za-z0-9._-]{0,127})?@sha256:[a-f0-9]{64}$/u;
const digestPattern = /^sha256:[a-f0-9]{64}$/u;
const imageIdPattern = /^(?:sha256:)?[a-f0-9]{64}$/u;
const containerIdPattern = /^[a-f0-9]{12,64}$/u;
const toolNamePattern = /^[a-z0-9][a-z0-9+._-]*$/u;
const environmentNamePattern = /^[A-Z][A-Z0-9_]*$/u;
const fingerprintPattern = /^[a-f0-9]{64}$/u;
const maximumSecurityProfileBytes = 4 * 1024 * 1024;
const supportedContainerRuntimes = new Set(["crun", "runc"]);
const forwardedRuntimeEnvironmentNames = [
  "HOME",
  "LOGNAME",
  "USER",
  "XDG_DATA_HOME",
  "XDG_RUNTIME_DIR",
];
export const ociSeccompProfilePath = "/usr/share/containers/seccomp.json";
const architectureNames = Object.freeze({
  aarch64: "arm64",
  arm64: "arm64",
  amd64: "amd64",
  x86_64: "amd64",
});
const reportArchitectureNames = Object.freeze({
  arm64: "aarch64",
  amd64: "x86_64",
});

function requireObject(value, name) {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    throw new TypeError(`${name} must be an object`);
  }
  return value;
}

function requireSuccessfulResult(result, name) {
  if (result.error || (result.signal ?? null) !== null || result.status !== 0) {
    throw new TypeError(`${name} failed`);
  }
  return result;
}

function parseJsonResult(result, name) {
  requireSuccessfulResult(result, name);
  if (
    typeof result.stdout !== "string" ||
    result.stdout.length === 0 ||
    result.stdout.length > maximumOutputBytes ||
    result.stdout.includes("\0")
  ) {
    throw new TypeError(`${name} returned invalid JSON`);
  }
  try {
    return JSON.parse(result.stdout);
  } catch {
    throw new TypeError(`${name} returned invalid JSON`);
  }
}

function probeOptions(environment) {
  return {
    encoding: "utf8",
    env: environment,
    killSignal: "SIGKILL",
    maxBuffer: maximumOutputBytes,
    stdio: ["ignore", "pipe", "pipe"],
    timeout: probeTimeoutMs,
  };
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

function firstOutputLine(result, name) {
  requireSuccessfulResult(result, name);
  const version = `${result.stdout ?? ""}\n${result.stderr ?? ""}`
    .split(/\r?\n/u)
    .map((line) => line.trim())
    .find((line) => line !== "");
  if (!version || version.length > 1024 || version.includes("\0")) {
    throw new TypeError(`${name} returned an invalid version`);
  }
  return version;
}

function imageDigest(image) {
  if (typeof image !== "string" || !imagePattern.test(image)) {
    throw new TypeError("OCI image reference must be digest-pinned");
  }
  return image.slice(image.lastIndexOf("@") + 1);
}

function normalizeImageId(value) {
  if (typeof value !== "string" || !imageIdPattern.test(value)) {
    throw new TypeError("OCI image inspect returned an invalid image ID");
  }
  return value.startsWith("sha256:") ? value : `sha256:${value}`;
}

function normalizeArchitecture(value, names, name) {
  const normalized = names[value];
  if (!normalized) throw new TypeError(`${name} architecture is unsupported`);
  return normalized;
}

function requireSafeMountPath(path, name) {
  if (
    typeof path !== "string" ||
    path === "" ||
    path.includes("\0") ||
    path.includes(",")
  ) {
    throw new TypeError(`${name} cannot be represented safely as an OCI mount`);
  }
  return resolve(path);
}

function mountArgument(source, destination, {
  readOnly,
  relabel,
}) {
  const safeSource = requireSafeMountPath(source, "OCI mount source");
  if (
    typeof destination !== "string" ||
    !destination.startsWith("/") ||
    destination.includes(",") ||
    destination.includes("\0")
  ) {
    throw new TypeError("OCI mount destination is invalid");
  }
  return [
    "--mount",
    [
      "type=bind",
      `src=${safeSource}`,
      `dst=${destination}`,
      `ro=${readOnly}`,
      `relabel=${relabel}`,
      "bind-nonrecursive",
    ].join(","),
  ];
}

function requireEnvironment(environment) {
  requireObject(environment, "OCI command environment");
  for (const [name, value] of Object.entries(environment)) {
    if (
      !environmentNamePattern.test(name) ||
      typeof value !== "string" ||
      value.includes("\0")
    ) {
      throw new TypeError("OCI command environment is invalid");
    }
  }
  return environment;
}

function requireRuntimeEnvironment(environment) {
  requireObject(environment, "OCI runtime environment");
  for (const [name, value] of Object.entries(environment)) {
    if (
      name === "" ||
      name.includes("=") ||
      name.includes("\0") ||
      typeof value !== "string" ||
      value.includes("\0")
    ) {
      throw new TypeError("OCI runtime environment is invalid");
    }
  }
  return environment;
}

function requireRuntimeComponent(component, role) {
  requireObject(component, `OCI ${role} contract`);
  if (
    typeof component.name !== "string" ||
    !toolNamePattern.test(component.name) ||
    component.executable !== `/usr/bin/${component.name}` ||
    typeof component.version !== "string" ||
    component.version.length === 0 ||
    component.version.length > 1024 ||
    component.version.includes("\0") ||
    !Array.isArray(component.versionArgs) ||
    component.versionArgs.length === 0 ||
    component.versionArgs.some((argument) =>
      typeof argument !== "string" ||
      argument.length === 0 ||
      argument.includes("\0"))
  ) {
    throw new TypeError(`OCI ${role} contract is invalid`);
  }
  if (
    role === "container runtime"
      ? !supportedContainerRuntimes.has(component.name)
      : component.name !== "conmon"
  ) {
    throw new TypeError(`OCI ${role} is unsupported`);
  }
  return component;
}

function ociRuntimeConfiguration({
  containerRuntimeName,
  containerRuntimePath,
  monitorPath,
}) {
  return `[containers]\n` +
    `annotations = []\n` +
    `base_hosts_file = "none"\n` +
    `cgroups = "enabled"\n` +
    `cgroupns = "private"\n` +
    `default_capabilities = []\n` +
    `default_sysctls = []\n` +
    `default_ulimits = []\n` +
    `devices = []\n` +
    `env = []\n` +
    `env_host = false\n` +
    `http_proxy = false\n` +
    `ipcns = "none"\n` +
    `keyring = false\n` +
    `label = true\n` +
    `log_driver = "none"\n` +
    `mounts = []\n` +
    `netns = "none"\n` +
    `no_hosts = true\n` +
    `pidns = "private"\n` +
    `pids_limit = ${ociProcessLimit}\n` +
    `privileged = false\n` +
    `read_only = true\n` +
    `seccomp_profile = ${JSON.stringify(ociSeccompProfilePath)}\n` +
    `umask = "0077"\n` +
    `utsns = "private"\n` +
    `volumes = []\n\n` +
    `[engine]\n` +
    `cdi_spec_dirs = []\n` +
    `cgroup_manager = "systemd"\n` +
    `conmon_env_vars = []\n` +
    `conmon_path = [${JSON.stringify(monitorPath)}]\n` +
    `env = []\n` +
    `events_logger = "none"\n` +
    `hooks_dir = []\n` +
    `pull_policy = "never"\n` +
    `remote = false\n` +
    `runtime = ${JSON.stringify(containerRuntimeName)}\n\n` +
    `[engine.runtimes]\n` +
    `${containerRuntimeName} = [${JSON.stringify(containerRuntimePath)}]\n`;
}

export function createOciRuntimeEnvironment({
  stateRoot,
  containerRuntime,
  monitor,
  expectedConfigurationSha256,
  environment = process.env,
}) {
  requireRuntimeComponent(containerRuntime, "container runtime");
  requireRuntimeComponent(monitor, "monitor");
  const safeStateRoot = requireSafeMountPath(stateRoot, "OCI state root");
  const stateMetadata = lstatSync(safeStateRoot);
  const userId = process.getuid?.();
  if (
    !Number.isSafeInteger(userId) ||
    userId < 1 ||
    stateMetadata.isSymbolicLink() ||
    !stateMetadata.isDirectory() ||
    stateMetadata.uid !== userId ||
    (stateMetadata.mode & 0o077) !== 0
  ) {
    throw new TypeError(
      "OCI state root must be a private directory owned by a non-root user",
    );
  }
  const configuration = ociRuntimeConfiguration({
    containerRuntimeName: containerRuntime.name,
    containerRuntimePath: containerRuntime.executable,
    monitorPath: monitor.executable,
  });
  const configurationSha256 = createHash("sha256")
    .update(configuration)
    .digest("hex");
  if (
    !fingerprintPattern.test(expectedConfigurationSha256) ||
    configurationSha256 !== expectedConfigurationSha256
  ) {
    throw new TypeError("OCI runtime configuration fingerprint does not match");
  }
  const configurationPath = resolve(safeStateRoot, "containers.conf");
  const configurationHome = resolve(safeStateRoot, "xdg-config");
  mkdirSync(configurationHome, { mode: 0o700 });
  writeFileSync(configurationPath, configuration, {
    encoding: "utf8",
    flag: "wx",
    mode: 0o600,
  });
  const hostEnvironment = requireRuntimeEnvironment(environment);
  const runtimeEnvironment = {
    CONTAINERS_CONF: configurationPath,
    LANG: "C",
    LC_ALL: "C",
    PATH: "/usr/bin:/bin",
    TMPDIR: safeStateRoot,
    XDG_CONFIG_HOME: configurationHome,
  };
  for (const name of forwardedRuntimeEnvironmentNames) {
    if (Object.hasOwn(hostEnvironment, name)) {
      runtimeEnvironment[name] = hostEnvironment[name];
    }
  }
  return {
    configurationPath,
    configurationSha256,
    environment: runtimeEnvironment,
  };
}

export function ociRuntimeConfigurationFingerprint({
  containerRuntime,
  monitor,
}) {
  requireRuntimeComponent(containerRuntime, "container runtime");
  requireRuntimeComponent(monitor, "monitor");
  return createHash("sha256")
    .update(ociRuntimeConfiguration({
      containerRuntimeName: containerRuntime.name,
      containerRuntimePath: containerRuntime.executable,
      monitorPath: monitor.executable,
    }))
    .digest("hex");
}

export function inspectOciSeccompProfile({
  expectedSha256,
  path = ociSeccompProfilePath,
}) {
  if (!fingerprintPattern.test(expectedSha256)) {
    throw new TypeError("OCI seccomp profile fingerprint is invalid");
  }
  const realPath = realpathSync(path);
  const metadata = lstatSync(realPath);
  if (
    !metadata.isFile() ||
    metadata.uid !== 0 ||
    (metadata.mode & 0o022) !== 0 ||
    metadata.size < 1 ||
    metadata.size > maximumSecurityProfileBytes ||
    !realPath.startsWith("/usr/")
  ) {
    throw new TypeError("OCI seccomp profile is not a trusted system file");
  }
  const sha256 = createHash("sha256")
    .update(readFileSync(realPath))
    .digest("hex");
  if (sha256 !== expectedSha256) {
    throw new TypeError("OCI seccomp profile fingerprint does not match");
  }
  return { path, sha256 };
}

export function inspectOciRuntime({
  runtimePath,
  expectedVersion,
  versionArgs,
  containerRuntime,
  monitor,
  expectedSeccompSha256,
  spawn = spawnSync,
  environment = process.env,
  userId = process.getuid?.(),
  inspectSeccompProfile = inspectOciSeccompProfile,
}) {
  if (!Number.isSafeInteger(userId) || userId < 1) {
    throw new TypeError("OCI validation requires a non-root host user");
  }
  requireRuntimeComponent(containerRuntime, "container runtime");
  requireRuntimeComponent(monitor, "monitor");
  const versionResult = spawn(
    runtimePath,
    versionArgs,
    probeOptions(environment),
  );
  const version = firstOutputLine(versionResult, "OCI runtime version probe");
  if (!versionMatches(version, expectedVersion)) {
    throw new TypeError(
      `OCI runtime version does not match validation environment ` +
      `(${expectedVersion} required)`,
    );
  }
  const containerRuntimeVersion = firstOutputLine(
    spawn(
      containerRuntime.executable,
      containerRuntime.versionArgs,
      probeOptions(environment),
    ),
    "OCI container runtime version probe",
  );
  if (!versionMatches(containerRuntimeVersion, containerRuntime.version)) {
    throw new TypeError(
      "OCI container runtime version does not match its contract",
    );
  }
  const monitorVersion = firstOutputLine(
    spawn(
      monitor.executable,
      monitor.versionArgs,
      probeOptions(environment),
    ),
    "OCI monitor version probe",
  );
  if (!versionMatches(monitorVersion, monitor.version)) {
    throw new TypeError("OCI monitor version does not match its contract");
  }
  const info = parseJsonResult(
    spawn(runtimePath, ["info", "--format=json"], probeOptions(environment)),
    "OCI runtime inspection",
  );
  const host = requireObject(info.host, "OCI runtime host information");
  const security = requireObject(
    host.security,
    "OCI runtime security information",
  );
  const observedContainerRuntime = requireObject(
    host.ociRuntime,
    "OCI container runtime information",
  );
  const observedMonitor = requireObject(
    host.conmon,
    "OCI monitor information",
  );
  if (
    security.rootless !== true ||
    security.seccompEnabled !== true ||
    security.seccompProfilePath !== ociSeccompProfilePath ||
    host.serviceIsRemote !== false ||
    host.cgroupVersion !== "v2" ||
    host.cgroupManager !== "systemd" ||
    host.eventLogger !== "none" ||
    host.logDriver !== "none" ||
    !Array.isArray(info.plugins?.log) ||
    !info.plugins.log.includes("none")
  ) {
    throw new TypeError(
      "OCI runtime must be local, rootless, seccomp-enabled, cgroup v2, " +
      "and support non-persistent logs",
    );
  }
  if (
    observedContainerRuntime.name !== containerRuntime.name ||
    observedContainerRuntime.path !== containerRuntime.executable ||
    !versionMatches(observedContainerRuntime.version, containerRuntime.version)
  ) {
    throw new TypeError("OCI container runtime does not match its contract");
  }
  if (
    observedMonitor.path !== monitor.executable ||
    !versionMatches(observedMonitor.version, monitor.version)
  ) {
    throw new TypeError("OCI monitor does not match its contract");
  }
  const seccompProfile = inspectSeccompProfile({
    expectedSha256: expectedSeccompSha256,
    path: ociSeccompProfilePath,
  });
  return {
    architecture: normalizeArchitecture(
      host.arch,
      reportArchitectureNames,
      "OCI runtime",
    ),
    executable: runtimePath,
    name: "podman",
    containerRuntime: {
      executable: containerRuntime.executable,
      name: containerRuntime.name,
      version: containerRuntimeVersion,
      versionArgv: [
        containerRuntime.executable,
        ...containerRuntime.versionArgs,
      ],
    },
    monitor: {
      executable: monitor.executable,
      name: monitor.name,
      version: monitorVersion,
      versionArgv: [monitor.executable, ...monitor.versionArgs],
    },
    rootless: true,
    seccompProfile,
    version,
    versionArgv: [runtimePath, ...versionArgs],
  };
}

export function inspectOciImage({
  runtimePath,
  execution,
  expectedArchitecture,
  spawn = spawnSync,
  environment = process.env,
}) {
  requireObject(execution, "OCI execution contract");
  const expectedDigest = imageDigest(execution.image);
  const inspected = parseJsonResult(
    spawn(
      runtimePath,
      ["image", "inspect", execution.image],
      probeOptions(environment),
    ),
    "OCI image inspection",
  );
  if (!Array.isArray(inspected) || inspected.length !== 1) {
    throw new TypeError("OCI image inspection must return exactly one image");
  }
  const image = requireObject(inspected[0], "OCI image metadata");
  const observedDigests = [
    image.Digest,
    ...(Array.isArray(image.RepoDigests)
      ? image.RepoDigests.map((value) =>
        typeof value === "string" && value.includes("@")
          ? value.slice(value.lastIndexOf("@") + 1)
          : null)
      : []),
  ].filter((value) => typeof value === "string" && digestPattern.test(value));
  if (!observedDigests.includes(expectedDigest)) {
    throw new TypeError("local OCI image digest does not match its contract");
  }
  const architecture = normalizeArchitecture(
    image.Architecture,
    reportArchitectureNames,
    "OCI image",
  );
  if (architecture !== expectedArchitecture || image.Os !== "linux") {
    throw new TypeError("OCI image platform does not match its environment");
  }
  const labels = requireObject(image.Labels, "OCI image labels");
  if (
    labels["org.opencontainers.image.source"] !== execution.source ||
    labels["org.opencontainers.image.revision"] !== execution.revision
  ) {
    throw new TypeError("OCI image provenance does not match its contract");
  }
  return {
    architecture,
    digest: expectedDigest,
    id: normalizeImageId(image.Id ?? image.ID),
    operatingSystem: "linux",
    reference: execution.image,
    revision: execution.revision,
    source: execution.source,
  };
}

export function buildOciInvocation({
  runtimePath,
  execution,
  inputRoot,
  buildRoot,
  manifest,
  profile,
  command,
  environment,
  cidFile,
  serviceRoot = null,
  serviceWritable = false,
}) {
  if (!command || !["compile", "test"].includes(command.phase)) {
    throw new TypeError("unsupported OCI validation phase");
  }
  if (typeof serviceWritable !== "boolean") {
    throw new TypeError("serviceWritable must be boolean");
  }
  if (
    !manifest?.paths ||
    typeof manifest.paths.build !== "string" ||
    !/^[A-Za-z0-9._/-]+$/u.test(manifest.paths.build) ||
    manifest.paths.build.split("/").some((segment) =>
      segment === "" || segment === "." || segment === "..")
  ) {
    throw new TypeError("OCI validation build path is invalid");
  }
  imageDigest(execution.image);
  const limits = profile.sandbox.resourceLimits[command.phase];
  const memorySwapBytes = limits.addressSpaceBytes + 1;
  if (!Number.isSafeInteger(memorySwapBytes)) {
    throw new TypeError("OCI validation memory limit is invalid");
  }
  const platformArchitecture = normalizeArchitecture(
    profile.environment.host.architecture,
    architectureNames,
    "validation environment",
  );
  const args = [
    "--events-backend=none",
    "run",
    "--rm",
    "--pull=never",
    `--cidfile=${requireSafeMountPath(cidFile, "OCI cidfile")}`,
    "--network=none",
    "--ipc=none",
    "--pid=private",
    "--cgroupns=private",
    "--uts=private",
    `--userns=keep-id:uid=${ociUserId},gid=${ociUserId}`,
    `--user=${ociUserId}:${ociUserId}`,
    "--privileged=false",
    "--cap-drop=all",
    "--security-opt=no-new-privileges",
    `--security-opt=seccomp=${ociSeccompProfilePath}`,
    "--read-only",
    "--read-only-tmpfs=false",
    "--image-volume=ignore",
    "--no-healthcheck",
    "--no-hosts",
    "--http-proxy=false",
    "--env-host=false",
    "--unsetenv-all",
    "--log-driver=none",
    "--entrypoint=",
    "--stop-timeout=1",
    "--systemd=false",
    "--umask=077",
    "--hostname=benchmark-sandbox",
    "--workdir=/workspace",
    `--platform=linux/${platformArchitecture}`,
    `--memory=${limits.addressSpaceBytes}b`,
    `--memory-swap=${memorySwapBytes}b`,
    `--pids-limit=${ociProcessLimit}`,
    `--ulimit=as=${limits.addressSpaceBytes}:${limits.addressSpaceBytes}`,
    `--ulimit=cpu=${limits.cpuSeconds}:${limits.cpuSeconds}`,
    `--ulimit=fsize=${limits.fileBytes}:${limits.fileBytes}`,
    `--ulimit=nofile=${limits.openFiles}:${limits.openFiles}`,
    "--ulimit=core=0:0",
    `--tmpfs=/tmp:rw,noexec,nosuid,nodev,size=` +
      `${profile.sandbox.temporaryDirectoryBytes}`,
    `--tmpfs=/run:rw,noexec,nosuid,nodev,size=` +
      `${profile.sandbox.rootTmpfsBytes}`,
    ...mountArgument(inputRoot, "/workspace", {
      readOnly: true,
      relabel: "shared",
    }),
    ...mountArgument(buildRoot, `/workspace/${manifest.paths.build}`, {
      readOnly: command.phase === "test",
      relabel: serviceRoot === null ? "private" : "shared",
    }),
  ];
  if (serviceRoot !== null) {
    const source = serviceWritable ? serviceRoot : resolve(serviceRoot, "socket");
    const destination = serviceWritable
      ? "/workspace/service"
      : "/workspace/service/socket";
    args.push(...mountArgument(source, destination, {
      readOnly: !serviceWritable,
      relabel: "shared",
    }));
  }
  for (const [name, value] of Object.entries(requireEnvironment(environment))
    .sort(([left], [right]) => left.localeCompare(right))) {
    args.push(`--env=${name}=${value}`);
  }
  args.push(execution.image, ...command.argv);
  return {
    args,
    cidFile: resolve(cidFile),
    command: runtimePath,
    options: {
      encoding: "utf8",
      killSignal: "SIGKILL",
      maxBuffer: maximumOutputBytes,
      stdio: ["ignore", "pipe", "pipe"],
      timeout: command.timeoutMs,
    },
  };
}

export function runOciInvocation(invocation, {
  spawn = spawnSync,
  cleanupSpawn = spawnSync,
  environment = process.env,
} = {}) {
  try {
    return spawn(
      invocation.command,
      invocation.args,
      {
        ...invocation.options,
        env: environment,
      },
    );
  } finally {
    cleanupOciContainer({
      runtimePath: invocation.command,
      cidFile: invocation.cidFile,
      spawn: cleanupSpawn,
      environment,
    });
  }
}

export function cleanupOciContainer({
  runtimePath,
  cidFile,
  spawn = spawnSync,
  environment = process.env,
}) {
  if (!existsSync(cidFile)) return;
  const containerId = readFileSync(cidFile, "utf8").trim();
  if (!containerIdPattern.test(containerId)) {
    throw new TypeError(
      `OCI runtime wrote an invalid container ID; recovery record remains ` +
      `at ${resolve(cidFile)}`,
    );
  }
  const cleanup = spawn(
    runtimePath,
    [
      "--events-backend=none",
      "rm",
      "--force",
      "--time=0",
      "--ignore",
      containerId,
    ],
    probeOptions(environment),
  );
  if (
    cleanup.error ||
    (cleanup.signal ?? null) !== null ||
    cleanup.status !== 0
  ) {
    throw new TypeError(
      `OCI runtime could not clean up its container; recovery ID remains ` +
      `at ${resolve(cidFile)}`,
    );
  }
  rmSync(cidFile, { force: true });
}

export function removeOciStateRoot(stateRoot) {
  const safeStateRoot = resolve(stateRoot);
  if (!existsSync(safeStateRoot)) return true;
  const metadata = lstatSync(safeStateRoot);
  if (metadata.isSymbolicLink() || !metadata.isDirectory()) {
    throw new TypeError("OCI state root must be a non-symlink directory");
  }
  if (readdirSync(safeStateRoot).some((entry) => entry.endsWith(".cid"))) {
    return false;
  }
  rmSync(safeStateRoot, { recursive: true });
  return true;
}

export function inspectOciToolchain({
  runtimePath,
  execution,
  inputRoot,
  buildRoot,
  manifest,
  profile,
  name,
  versionArgs,
  expectedVersion,
  environment,
  runtimeEnvironment = process.env,
  cidFile,
  spawn = spawnSync,
  cleanupSpawn = spawnSync,
}) {
  if (typeof name !== "string" || !toolNamePattern.test(name)) {
    throw new TypeError("OCI toolchain name is invalid");
  }
  const invocation = buildOciInvocation({
    runtimePath,
    execution,
    inputRoot,
    buildRoot,
    manifest,
    profile,
    command: {
      argv: [name, ...versionArgs],
      id: "oci-toolchain-probe",
      phase: "test",
      requiredTools: [name],
      timeoutMs: probeTimeoutMs,
    },
    environment,
    cidFile,
  });
  const result = runOciInvocation(invocation, {
    spawn,
    cleanupSpawn,
    environment: runtimeEnvironment,
  });
  const version = firstOutputLine(result, `OCI ${name} version probe`);
  if (!versionMatches(version, expectedVersion)) {
    throw new TypeError(
      `${name} version does not match validation profile ` +
      `(${expectedVersion} required)`,
    );
  }
  return {
    executable: `oci:${name}`,
    name,
    version,
    versionArgv: [name, ...versionArgs],
  };
}

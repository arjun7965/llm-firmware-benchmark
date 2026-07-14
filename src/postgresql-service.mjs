import { spawnSync } from "node:child_process";
import {
  mkdirSync,
  mkdtempSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const maximumOutputBytes = 1024 * 1024;
export const postgresqlServiceWorkspace = "/workspace/service";
const supervisorPath = fileURLToPath(
  new URL("./service-runtime-supervisor.mjs", import.meta.url),
);

export function postgresqlServiceEnvironment(socketDirectory) {
  return {
    LANG: "C",
    LC_ALL: "C",
    PGDATABASE: "postgres",
    PGHOST: socketDirectory,
    PGUSER: "benchmark",
  };
}

export function replacePostgresqlServiceWorkspace(argv, serviceRoot) {
  return argv.map((argument) =>
    argument === postgresqlServiceWorkspace
      ? serviceRoot
      : argument.startsWith(`${postgresqlServiceWorkspace}/`)
        ? `${serviceRoot}${argument.slice(postgresqlServiceWorkspace.length)}`
        : argument);
}

export function createPostgresqlServiceRunRoot({
  temporaryRoot = null,
} = {}) {
  const runRoot = temporaryRoot
    ? mkdtempSync(join(temporaryRoot, "postgresql-service-"))
    : mkdtempSync(join(tmpdir(), "postgresql-service-"));
  const serviceRoot = join(runRoot, "service");
  mkdirSync(join(serviceRoot, "socket"), {
    recursive: true,
    mode: 0o700,
  });
  return { runRoot, serviceRoot };
}

export function removePostgresqlServiceRunRoot(runRoot) {
  rmSync(runRoot, { recursive: true, force: true });
}

export function runPostgresqlService({
  candidate,
  initialize,
  logPath,
  ready,
  runRoot,
  shutdownTimeoutMs,
  spawn = spawnSync,
  start,
  startupTimeoutMs,
  stop = null,
}) {
  const configurationPath = join(runRoot, "supervisor.json");
  const configuration = {
    candidate,
    initialize,
    logPath,
    ready,
    shutdownTimeoutMs,
    start,
    startupTimeoutMs,
    stop,
  };
  writeFileSync(
    configurationPath,
    `${JSON.stringify(configuration)}\n`,
    { encoding: "utf8", mode: 0o600 },
  );
  const shutdownBudgetMs = shutdownTimeoutMs * (stop === null ? 1 : 2);
  const totalTimeoutMs = startupTimeoutMs + candidate.timeoutMs +
    shutdownBudgetMs + 5_000;
  try {
    return spawn(process.execPath, [supervisorPath, configurationPath], {
      cwd: dirname(supervisorPath),
      encoding: "utf8",
      killSignal: "SIGKILL",
      maxBuffer: maximumOutputBytes,
      stdio: ["ignore", "pipe", "pipe"],
      timeout: totalTimeoutMs,
    });
  } finally {
    rmSync(configurationPath, { force: true });
  }
}

function localInvocation(argv, {
  cwd,
  env,
  serviceRoot,
  timeoutMs,
}) {
  const translated = replacePostgresqlServiceWorkspace(argv, serviceRoot);
  return {
    command: translated[0],
    args: translated.slice(1),
    cwd,
    env,
    timeoutMs,
  };
}

export function runLocalPostgresqlCommand({
  args,
  command,
  cwd,
  profile,
  spawn = spawnSync,
  temporaryRoot = null,
  timeoutMs,
}) {
  const service = profile.testRuntime?.service;
  if (service?.kind !== "postgresql") {
    throw new TypeError("PostgreSQL service profile is required");
  }
  const { runRoot, serviceRoot } = createPostgresqlServiceRunRoot({
    temporaryRoot,
  });
  const env = postgresqlServiceEnvironment(join(serviceRoot, "socket"));
  try {
    return runPostgresqlService({
      candidate: {
        command,
        args,
        cwd,
        env,
        timeoutMs,
      },
      initialize: localInvocation(service.initializeArgv, {
        cwd,
        env,
        serviceRoot,
        timeoutMs: service.startupTimeoutMs,
      }),
      logPath: join(serviceRoot, "postgres.log"),
      ready: localInvocation(service.readyArgv, {
        cwd,
        env,
        serviceRoot,
        timeoutMs: Math.min(service.startupTimeoutMs, 1_000),
      }),
      runRoot,
      shutdownTimeoutMs: service.shutdownTimeoutMs,
      spawn,
      start: localInvocation(service.startArgv, {
        cwd,
        env,
        serviceRoot,
        timeoutMs: service.startupTimeoutMs,
      }),
      startupTimeoutMs: service.startupTimeoutMs,
      stop: localInvocation(service.stopArgv, {
        cwd,
        env,
        serviceRoot,
        timeoutMs: service.shutdownTimeoutMs,
      }),
    });
  } finally {
    removePostgresqlServiceRunRoot(runRoot);
  }
}

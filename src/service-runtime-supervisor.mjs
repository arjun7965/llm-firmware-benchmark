import { spawn, spawnSync } from "node:child_process";
import {
  closeSync,
  openSync,
  readFileSync,
  writeSync,
} from "node:fs";

const maximumOutputBytes = 1024 * 1024;
const pollIntervalMs = 50;

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

function requirePositiveInteger(value, name) {
  if (!Number.isSafeInteger(value) || value < 1) {
    throw new TypeError(`${name} must be a positive safe integer`);
  }
}

function validateInvocation(invocation, name) {
  requireExactKeys(
    invocation,
    ["args", "command", "cwd", "env", "timeoutMs"],
    name,
  );
  if (
    typeof invocation.command !== "string" ||
    invocation.command === "" ||
    !Array.isArray(invocation.args) ||
    invocation.args.some((argument) =>
      typeof argument !== "string" || argument.includes("\0")) ||
    typeof invocation.cwd !== "string" ||
    invocation.cwd === "" ||
    !invocation.env ||
    typeof invocation.env !== "object" ||
    Array.isArray(invocation.env) ||
    Object.entries(invocation.env).some(([key, value]) =>
      !/^[A-Z][A-Z0-9_]*$/u.test(key) ||
      typeof value !== "string" ||
      value.includes("\0"))
  ) {
    throw new TypeError(`${name} is invalid`);
  }
  requirePositiveInteger(invocation.timeoutMs, `${name} timeoutMs`);
  return invocation;
}

function loadConfiguration(path) {
  const configuration = JSON.parse(readFileSync(path, "utf8"));
  requireExactKeys(
    configuration,
    [
      "candidate",
      "initialize",
      "logPath",
      "ready",
      "shutdownTimeoutMs",
      "start",
      "startupTimeoutMs",
      "stop",
    ],
    "service supervisor configuration",
  );
  validateInvocation(configuration.initialize, "initialize invocation");
  validateInvocation(configuration.start, "start invocation");
  validateInvocation(configuration.ready, "ready invocation");
  validateInvocation(configuration.candidate, "candidate invocation");
  if (configuration.stop !== null) {
    validateInvocation(configuration.stop, "stop invocation");
  }
  if (
    typeof configuration.logPath !== "string" ||
    configuration.logPath === ""
  ) {
    throw new TypeError("service supervisor logPath is invalid");
  }
  requirePositiveInteger(
    configuration.startupTimeoutMs,
    "service supervisor startupTimeoutMs",
  );
  requirePositiveInteger(
    configuration.shutdownTimeoutMs,
    "service supervisor shutdownTimeoutMs",
  );
  return configuration;
}

function invocationOptions(invocation, overrides = {}) {
  return {
    cwd: invocation.cwd,
    env: {
      ...process.env,
      ...invocation.env,
    },
    killSignal: "SIGKILL",
    timeout: invocation.timeoutMs,
    ...overrides,
  };
}

function runSync(invocation) {
  return spawnSync(
    invocation.command,
    invocation.args,
    invocationOptions(invocation, {
      encoding: "utf8",
      maxBuffer: maximumOutputBytes,
      stdio: ["ignore", "pipe", "pipe"],
    }),
  );
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

function completedSuccessfully(result) {
  return !result.error && result.signal === null && result.status === 0;
}

function sleep(milliseconds) {
  return new Promise((resolve) => setTimeout(resolve, milliseconds));
}

async function waitForExit(child, timeoutMs) {
  if (child.exitCode !== null || child.signalCode !== null) return true;
  let timeout;
  const exited = await Promise.race([
    new Promise((resolve) => child.once("exit", () => resolve(true))),
    new Promise((resolve) => {
      timeout = setTimeout(() => resolve(false), timeoutMs);
    }),
  ]);
  clearTimeout(timeout);
  return exited;
}

function writeResult(result) {
  if (result.stdout) writeSync(1, result.stdout);
  if (result.stderr) writeSync(2, result.stderr);
  if (result.error) {
    writeSync(
      2,
      `${result.error.code ?? "ERROR"}: ${result.error.message}\n`,
    );
  }
  if (Number.isInteger(result.status)) return result.status;
  return 1;
}

async function main() {
  if (process.argv.length !== 3) {
    throw new TypeError("expected one service supervisor configuration path");
  }
  const configuration = loadConfiguration(process.argv[2]);
  const initialized = runSync(configuration.initialize);
  if (!completedSuccessfully(initialized)) {
    writeSync(
      2,
      `PostgreSQL initialization failed\n${diagnostics(initialized)}\n`,
    );
    return 1;
  }

  const logDescriptor = openSync(configuration.logPath, "a", 0o600);
  const server = spawn(
    configuration.start.command,
    configuration.start.args,
    invocationOptions(configuration.start, {
      stdio: ["ignore", logDescriptor, logDescriptor],
      timeout: 0,
    }),
  );
  let serverError = null;
  server.once("error", (error) => {
    serverError = error;
  });
  let candidateResult = null;
  try {
    const readinessDeadline = Date.now() + configuration.startupTimeoutMs;
    let readyResult = null;
    while (Date.now() < readinessDeadline) {
      if (
        serverError ||
        server.exitCode !== null ||
        server.signalCode !== null
      ) {
        break;
      }
      readyResult = runSync(configuration.ready);
      if (completedSuccessfully(readyResult)) break;
      await sleep(pollIntervalMs);
    }
    if (!readyResult || !completedSuccessfully(readyResult)) {
      const serverLog = readFileSync(configuration.logPath, "utf8");
      writeSync(
        2,
        `PostgreSQL did not become ready\n${diagnostics(readyResult ?? {})}` +
        `${serverError ? `\n${serverError.code ?? "ERROR"}: ` +
          serverError.message : ""}` +
        `${serverLog ? `\n${serverLog}` : ""}\n`,
      );
      return 1;
    }
    candidateResult = runSync(configuration.candidate);
  } finally {
    if (
      configuration.stop !== null &&
      !serverError &&
      server.exitCode === null &&
      server.signalCode === null
    ) {
      runSync(configuration.stop);
    }
    if (
      !serverError &&
      server.exitCode === null &&
      server.signalCode === null
    ) {
      server.kill("SIGTERM");
    }
    if (
      !serverError &&
      !await waitForExit(server, configuration.shutdownTimeoutMs)
    ) {
      server.kill("SIGKILL");
      await waitForExit(server, configuration.shutdownTimeoutMs);
    }
    closeSync(logDescriptor);
  }
  return writeResult(candidateResult);
}

try {
  process.exitCode = await main();
} catch (error) {
  writeSync(2, `${error.name}: ${error.message}\n`);
  process.exitCode = 1;
}

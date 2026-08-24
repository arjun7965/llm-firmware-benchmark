import { createHash } from "node:crypto";
import { spawnSync } from "node:child_process";
import {
  mkdtempSync,
  readFileSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { parseArgs } from "node:util";
import {
  readValidationHost,
  resolveExecutable,
} from "../src/fixture-sandbox.mjs";
import {
  createOciRuntimeEnvironment,
  inspectOciRuntime,
  ociRuntimeConfigurationFingerprint,
  ociSeccompProfilePath,
} from "../src/oci-sandbox.mjs";

const maximumOutputBytes = 1024 * 1024;

function commandVersion(executable, args, name) {
  const result = spawnSync(executable, args, {
    encoding: "utf8",
    env: {
      HOME: process.env.HOME ?? "/nonexistent",
      LANG: "C",
      LC_ALL: "C",
      PATH: "/usr/bin:/bin",
      XDG_RUNTIME_DIR: process.env.XDG_RUNTIME_DIR ?? "",
    },
    killSignal: "SIGKILL",
    maxBuffer: maximumOutputBytes,
    stdio: ["ignore", "pipe", "pipe"],
    timeout: 5_000,
  });
  if (result.error || result.signal !== null || result.status !== 0) {
    throw new TypeError(`${name} version probe failed`);
  }
  const firstLine = `${result.stdout}\n${result.stderr}`
    .split(/\r?\n/u)
    .map((line) => line.trim())
    .find((line) => line !== "");
  const match = firstLine?.match(/(?:^|\s)(\d+\.\d+(?:\.\d+)?)(?:\s|$)/u);
  if (!match) throw new TypeError(`${name} version probe is invalid`);
  return match[1];
}

const { values } = parseArgs({
  strict: true,
  options: { output: { type: "string" } },
});
if (!values.output) throw new TypeError("--output is required");

const runtimePath = resolveExecutable("podman", {
  pathValue: "/usr/bin:/bin",
});
const containerRuntimePath = resolveExecutable("crun", {
  pathValue: "/usr/bin:/bin",
});
const monitorPath = resolveExecutable("conmon", {
  pathValue: "/usr/bin:/bin",
});
const runtimeVersion = commandVersion(runtimePath, ["--version"], "Podman");
const containerRuntime = {
  name: "crun",
  executable: containerRuntimePath,
  version: commandVersion(containerRuntimePath, ["--version"], "crun"),
  versionArgs: ["--version"],
};
const monitor = {
  name: "conmon",
  executable: monitorPath,
  version: commandVersion(monitorPath, ["--version"], "conmon"),
  versionArgs: ["--version"],
};
const seccompSha256 = createHash("sha256")
  .update(readFileSync(ociSeccompProfilePath))
  .digest("hex");
const configurationSha256 = ociRuntimeConfigurationFingerprint({
  containerRuntime,
  monitor,
});
const stateRoot = mkdtempSync(join(tmpdir(), "oci-runtime-probe-"));

try {
  const prepared = createOciRuntimeEnvironment({
    stateRoot,
    containerRuntime,
    monitor,
    expectedConfigurationSha256: configurationSha256,
    environment: process.env,
  });
  const observed = inspectOciRuntime({
    runtimePath,
    expectedVersion: runtimeVersion,
    versionArgs: ["--version"],
    containerRuntime,
    monitor,
    expectedSeccompSha256: seccompSha256,
    environment: prepared.environment,
    userId: process.getuid(),
  });
  const contract = {
    schemaVersion: "1.0",
    runner: readValidationHost(),
    sandbox: {
      configurationSha256,
      containerRuntime,
      limiter: {
        name: "podman",
        executable: "podman",
        version: runtimeVersion,
        versionArgs: ["--version"],
      },
      monitor,
      runtime: {
        name: "podman",
        executable: "podman",
        version: runtimeVersion,
        versionArgs: ["--version"],
      },
      seccompProfile: observed.seccompProfile,
    },
  };
  writeFileSync(values.output, `${JSON.stringify(contract, null, 2)}\n`, {
    encoding: "utf8",
    flag: "wx",
    mode: 0o600,
  });
  console.log(JSON.stringify(contract, null, 2));
} finally {
  rmSync(stateRoot, { recursive: true, force: true });
}

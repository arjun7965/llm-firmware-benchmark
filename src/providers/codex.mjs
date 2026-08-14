import { spawn } from "node:child_process";
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

const defaultTimeoutMs = 600_000;
const supportedEfforts = new Set([
  "minimal",
  "low",
  "medium",
  "high",
  "xhigh",
  "max",
  "ultra",
]);
const supportedOptions = new Set(["effort", "timeoutMs"]);
const disabledFeatures = [
  "apps",
  "browser_use",
  "computer_use",
  "goals",
  "image_generation",
  "multi_agent",
  "plugins",
  "remote_plugin",
  "shell_tool",
  "skill_mcp_dependency_install",
  "skill_search",
  "tool_suggest",
];

function isPlainObject(value) {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    return false;
  }
  const prototype = Object.getPrototypeOf(value);
  return prototype === Object.prototype || prototype === null;
}

function readCodexOptions(job) {
  const options = job.modelOptions ?? {};
  if (!isPlainObject(options)) {
    throw new TypeError("Codex options must be an object");
  }
  const unknownOption = Object.keys(options)
    .find((key) => !supportedOptions.has(key));
  if (unknownOption) {
    throw new TypeError(`unsupported Codex option: ${unknownOption}`);
  }

  const effort = options.effort ?? "medium";
  if (!supportedEfforts.has(effort)) {
    throw new TypeError(`unsupported Codex effort: ${effort}`);
  }
  const timeoutMs = options.timeoutMs ?? defaultTimeoutMs;
  if (!Number.isInteger(timeoutMs) || timeoutMs < 1) {
    throw new TypeError("Codex timeoutMs must be a positive integer");
  }
  if (typeof job.modelId !== "string" || job.modelId.trim() === "") {
    throw new TypeError("Codex model identifier must be a non-empty string");
  }
  if (typeof job.task?.prompt !== "string" ||
      job.task.prompt.trim() === "") {
    throw new TypeError("Codex task prompt must be a non-empty string");
  }

  return { effort, timeoutMs };
}

export function buildCodexInvocation(job, {
  command = "codex",
  cwd,
} = {}) {
  if (typeof cwd !== "string" || cwd.trim() === "") {
    throw new TypeError("Codex cwd must be a non-empty string");
  }
  const { effort } = readCodexOptions(job);
  const disabledFeatureArgs = disabledFeatures.flatMap((feature) => [
    "--disable",
    feature,
  ]);

  return {
    command,
    args: [
      "exec",
      "--ephemeral",
      "--ignore-user-config",
      "--ignore-rules",
      "--strict-config",
      "--skip-git-repo-check",
      "--sandbox",
      "read-only",
      "--model",
      job.modelId,
      "--config",
      `model_reasoning_effort="${effort}"`,
      "--config",
      "web_search=\"disabled\"",
      ...disabledFeatureArgs,
      "--color",
      "never",
      job.task.prompt,
    ],
    options: {
      cwd,
      stdio: ["ignore", "pipe", "pipe"],
    },
  };
}

export async function executeCodexJob(job, {
  command = "codex",
  timeoutMs = job.modelOptions?.timeoutMs ?? defaultTimeoutMs,
  spawnImpl = spawn,
  createWorkingDirectory = () =>
    mkdtempSync(join(tmpdir(), "llm-benchmark-codex-")),
  removeWorkingDirectory = (path) =>
    rmSync(path, { recursive: true, force: true }),
} = {}) {
  if (!Number.isInteger(timeoutMs) || timeoutMs < 1) {
    throw new TypeError("Codex timeoutMs must be a positive integer");
  }

  let stdout = "";
  let stderr = "";
  let child;
  let cwd;
  let timer;
  let finished = false;

  return new Promise((resolve) => {
    function finish(exitCode, signal, error) {
      if (finished) return;
      finished = true;
      clearTimeout(timer);
      if (cwd !== undefined) {
        try {
          removeWorkingDirectory(cwd);
        } catch (cleanupError) {
          exitCode = exitCode === 0 ? 1 : exitCode;
          error = error
            ? `${error}; cleanup failed: ${cleanupError.message}`
            : `cleanup failed: ${cleanupError.message}`;
        }
      }
      resolve({ exitCode, signal, stdout, stderr, error });
    }

    try {
      cwd = createWorkingDirectory();
      const invocation = buildCodexInvocation(job, { command, cwd });
      child = spawnImpl(
        invocation.command,
        invocation.args,
        invocation.options,
      );
      child.stdout.setEncoding("utf8");
      child.stderr.setEncoding("utf8");
      child.stdout.on("data", (chunk) => { stdout += chunk; });
      child.stderr.on("data", (chunk) => { stderr += chunk; });
      child.on("error", (error) => finish(null, null, error.message));
      child.on("close", (exitCode, signal) => finish(exitCode, signal, null));
      timer = setTimeout(() => {
        try {
          child.kill("SIGTERM");
          finish(null, "SIGTERM", `timed out after ${timeoutMs} ms`);
        } catch (error) {
          finish(
            null,
            null,
            `timed out after ${timeoutMs} ms; kill failed: ${error.message}`,
          );
        }
      }, timeoutMs);
    } catch (error) {
      finish(null, null, error.message);
    }
  });
}

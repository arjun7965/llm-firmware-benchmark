import { spawn } from "node:child_process";
import { createHash } from "node:crypto";
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { extractAnswer } from "../answers.mjs";

const defaultTimeoutMs = 600_000;
const modelIdPattern = /^[^\s/]+\/[^\s/]+(?:\/[^\s/]+)*$/u;
const supportedOptions = new Set(["timeoutMs", "variant"]);
const benchmarkAgentPrompt = [
  "Answer the user's benchmark prompt directly without using tools.",
  "The prompt is self-contained. Do not inspect or modify files.",
  "Return the requested final answer in the exact format the user specifies.",
].join(" ");
const benchmarkConfiguration = {
  autoupdate: false,
  share: "disabled",
  snapshot: false,
  subagent_depth: 0,
  permission: {
    "*": "deny",
  },
  agent: {
    benchmark: {
      description: "Answer benchmark prompts without tools.",
      mode: "primary",
      prompt: benchmarkAgentPrompt,
      permission: {
        "*": "deny",
      },
    },
  },
};
const benchmarkConfig = JSON.stringify(benchmarkConfiguration);
const providerConfiguration = {
  schemaVersion: 1,
  invocation: {
    pure: true,
    format: "json",
    agent: "benchmark",
  },
  environment: {
    disableAutoupdate: true,
    disableGlobalConfig: true,
    disableLspDownload: true,
    disableProjectConfig: true,
    disableShare: true,
  },
  config: benchmarkConfiguration,
};

export const openCodeProviderConfigSha256 = createHash("sha256")
  .update(JSON.stringify(providerConfiguration))
  .digest("hex");

function isPlainObject(value) {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    return false;
  }
  const prototype = Object.getPrototypeOf(value);
  return prototype === Object.prototype || prototype === null;
}

function readOpenCodeOptions(job) {
  const options = job.modelOptions ?? {};
  if (!isPlainObject(options)) {
    throw new TypeError("OpenCode options must be an object");
  }
  const unknownOption = Object.keys(options)
    .find((key) => !supportedOptions.has(key));
  if (unknownOption) {
    throw new TypeError(`unsupported OpenCode option: ${unknownOption}`);
  }

  const timeoutMs = options.timeoutMs ?? defaultTimeoutMs;
  if (!Number.isInteger(timeoutMs) || timeoutMs < 1) {
    throw new TypeError("OpenCode timeoutMs must be a positive integer");
  }
  const variant = options.variant;
  if (variant !== undefined &&
      (typeof variant !== "string" || !/^\S+$/u.test(variant))) {
    throw new TypeError(
      "OpenCode variant must be a non-empty string without whitespace",
    );
  }
  if (typeof job.modelId !== "string" ||
      !modelIdPattern.test(job.modelId)) {
    throw new TypeError(
      "OpenCode model identifier must use provider/model format",
    );
  }
  if (typeof job.task?.prompt !== "string" ||
      job.task.prompt.trim() === "") {
    throw new TypeError("OpenCode task prompt must be a non-empty string");
  }

  return { timeoutMs, variant };
}

export function buildOpenCodeInvocation(job, {
  command = "opencode",
  cwd,
  environment = process.env,
} = {}) {
  if (typeof cwd !== "string" || cwd.trim() === "") {
    throw new TypeError("OpenCode cwd must be a non-empty string");
  }
  const { variant } = readOpenCodeOptions(job);
  const env = {
    ...environment,
    OPENCODE_CONFIG_CONTENT: benchmarkConfig,
    OPENCODE_CONFIG_DIR: join(cwd, ".opencode"),
    OPENCODE_DISABLE_AUTOUPDATE: "true",
    OPENCODE_DISABLE_GLOBAL_CONFIG: "true",
    OPENCODE_DISABLE_LSP_DOWNLOAD: "true",
    OPENCODE_DISABLE_PROJECT_CONFIG: "true",
    OPENCODE_DISABLE_SHARE: "true",
  };
  delete env.OPENCODE_CONFIG;
  delete env.OPENCODE_PERMISSION;

  return {
    command,
    args: [
      "--pure",
      "run",
      "--format",
      "json",
      "--model",
      job.modelId,
      "--agent",
      "benchmark",
      ...(variant === undefined ? [] : ["--variant", variant]),
      "--",
      job.task.prompt,
    ],
    options: {
      cwd,
      env,
      stdio: ["ignore", "pipe", "pipe"],
    },
  };
}

export async function executeOpenCodeJob(job, {
  command = "opencode",
  timeoutMs = job.modelOptions?.timeoutMs ?? defaultTimeoutMs,
  spawnImpl = spawn,
  createWorkingDirectory = () =>
    mkdtempSync(join(tmpdir(), "llm-benchmark-opencode-")),
  removeWorkingDirectory = (path) =>
    rmSync(path, { recursive: true, force: true }),
  environment = process.env,
} = {}) {
  if (!Number.isInteger(timeoutMs) || timeoutMs < 1) {
    throw new TypeError("OpenCode timeoutMs must be a positive integer");
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
      if (exitCode === 0 && error === null) {
        try {
          if (extractAnswer(stdout).trim() === "") {
            throw new TypeError("OpenCode output is empty");
          }
        } catch (outputError) {
          exitCode = 1;
          error =
            "OpenCode completed without an extractable text result: " +
            outputError.message;
        }
      }
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
      const invocation = buildOpenCodeInvocation(job, {
        command,
        cwd,
        environment,
      });
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

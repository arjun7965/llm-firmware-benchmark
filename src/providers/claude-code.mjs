import { spawn } from "node:child_process";
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

const defaultTimeoutMs = 600_000;
const supportedEfforts = new Set([
  "low",
  "medium",
  "high",
  "xhigh",
  "max",
  "ultracode",
]);
const supportedOptions = new Set(["effort", "timeoutMs"]);

function isPlainObject(value) {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    return false;
  }
  const prototype = Object.getPrototypeOf(value);
  return prototype === Object.prototype || prototype === null;
}

function readClaudeCodeOptions(job) {
  const options = job.modelOptions ?? {};
  if (!isPlainObject(options)) {
    throw new TypeError("Claude Code options must be an object");
  }
  const unknownOption = Object.keys(options)
    .find((key) => !supportedOptions.has(key));
  if (unknownOption) {
    throw new TypeError(`unsupported Claude Code option: ${unknownOption}`);
  }

  const effort = options.effort ?? "medium";
  if (!supportedEfforts.has(effort)) {
    throw new TypeError(`unsupported Claude Code effort: ${effort}`);
  }
  const timeoutMs = options.timeoutMs ?? defaultTimeoutMs;
  if (!Number.isInteger(timeoutMs) || timeoutMs < 1) {
    throw new TypeError("Claude Code timeoutMs must be a positive integer");
  }
  if (typeof job.modelId !== "string" || job.modelId.trim() === "") {
    throw new TypeError(
      "Claude Code model identifier must be a non-empty string",
    );
  }
  if (typeof job.task?.prompt !== "string" ||
      job.task.prompt.trim() === "") {
    throw new TypeError("Claude Code task prompt must be a non-empty string");
  }

  return { effort, timeoutMs };
}

export function buildClaudeCodeInvocation(job, {
  command = "claude",
  cwd,
  environment = process.env,
} = {}) {
  if (typeof cwd !== "string" || cwd.trim() === "") {
    throw new TypeError("Claude Code cwd must be a non-empty string");
  }
  const { effort } = readClaudeCodeOptions(job);

  return {
    command,
    args: [
      "--print",
      "--no-session-persistence",
      "--safe-mode",
      "--disable-slash-commands",
      "--no-chrome",
      "--strict-mcp-config",
      "--mcp-config",
      '{"mcpServers":{}}',
      "--tools",
      "",
      "--disallowedTools",
      "mcp__*",
      "--permission-mode",
      "dontAsk",
      "--max-turns",
      "1",
      "--model",
      job.modelId,
      "--effort",
      effort,
      "--output-format",
      "json",
      job.task.prompt,
    ],
    options: {
      cwd,
      env: {
        ...environment,
        CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC: "1",
        CLAUDE_CODE_SKIP_PROMPT_HISTORY: "1",
        ENABLE_CLAUDEAI_MCP_SERVERS: "false",
      },
      stdio: ["ignore", "pipe", "pipe"],
    },
  };
}

export async function executeClaudeCodeJob(job, {
  command = "claude",
  timeoutMs = job.modelOptions?.timeoutMs ?? defaultTimeoutMs,
  spawnImpl = spawn,
  createWorkingDirectory = () =>
    mkdtempSync(join(tmpdir(), "llm-benchmark-claude-code-")),
  removeWorkingDirectory = (path) =>
    rmSync(path, { recursive: true, force: true }),
  environment = process.env,
} = {}) {
  if (!Number.isInteger(timeoutMs) || timeoutMs < 1) {
    throw new TypeError("Claude Code timeoutMs must be a positive integer");
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
      const invocation = buildClaudeCodeInvocation(job, {
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

import { spawn } from "node:child_process";

export function buildNcodeInvocation(job, {
  command = "ncode",
  cwd = "/tmp",
} = {}) {
  const effort = job.modelOptions?.effort ?? "medium";
  if (!["low", "medium", "high", "max"].includes(effort)) {
    throw new TypeError(`unsupported NCode effort: ${effort}`);
  }

  return {
    command,
    args: [
      "--print",
      "--no-session-persistence",
      "--tools",
      "",
      "--effort",
      effort,
      "--model",
      job.modelId,
      "--output-format",
      "json",
      job.task.prompt,
    ],
    options: {
      cwd,
      stdio: ["ignore", "pipe", "pipe"],
    },
  };
}

export async function executeNcodeJob(job, {
  command = "ncode",
  cwd = "/tmp",
  timeoutMs = job.modelOptions?.timeoutMs ?? 300_000,
  spawnImpl = spawn,
} = {}) {
  if (!Number.isInteger(timeoutMs) || timeoutMs < 1) {
    throw new TypeError("NCode timeoutMs must be a positive integer");
  }
  const invocation = buildNcodeInvocation(job, { command, cwd });
  let stdout = "";
  let stderr = "";
  let child;
  let timer;
  let finished = false;

  return new Promise((resolve) => {
    function finish(exitCode, signal, error) {
      if (finished) return;
      finished = true;
      clearTimeout(timer);
      resolve({ exitCode, signal, stdout, stderr, error });
    }

    try {
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

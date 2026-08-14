import { EventEmitter } from "node:events";
import { PassThrough } from "node:stream";
import test from "node:test";
import assert from "node:assert/strict";
import {
  buildCodexInvocation,
  executeCodexJob,
} from "../src/providers/codex.mjs";
import { getProvider } from "../src/providers/index.mjs";

const job = {
  run: 1,
  task: {
    id: "task-one",
    category: "test",
    prompt: "Write a robust parser.",
  },
  provider: "codex",
  modelName: "gpt-5.6-luna",
  modelId: "gpt-5.6-luna",
  modelOptions: {},
};

function fakeChild() {
  const child = new EventEmitter();
  child.stdout = new PassThrough();
  child.stderr = new PassThrough();
  child.kill = () => true;
  return child;
}

test("Codex invocation isolates the prompt and disables tools", () => {
  const invocation = buildCodexInvocation(job, { cwd: "/tmp/codex-test" });

  assert.equal(invocation.command, "codex");
  assert.deepEqual(invocation.options, {
    cwd: "/tmp/codex-test",
    stdio: ["ignore", "pipe", "pipe"],
  });
  assert.deepEqual(invocation.args.slice(0, 14), [
    "exec",
    "--ephemeral",
    "--ignore-user-config",
    "--ignore-rules",
    "--strict-config",
    "--skip-git-repo-check",
    "--sandbox",
    "read-only",
    "--model",
    "gpt-5.6-luna",
    "--config",
    "model_reasoning_effort=\"medium\"",
    "--config",
    "web_search=\"disabled\"",
  ]);
  assert.equal(invocation.args.at(-1), "Write a robust parser.");
  assert.equal(invocation.args.includes("shell_tool"), true);
  assert.equal(invocation.args.includes("apps"), true);
  assert.equal(invocation.args.includes("multi_agent"), true);
});

test("Codex execution captures the final answer and removes its workspace", async () => {
  let invocation;
  const removed = [];
  const spawnImpl = (command, args, options) => {
    invocation = { command, args, options };
    const child = fakeChild();
    process.nextTick(() => {
      child.stdout.write("Use a state machine.\n");
      child.stderr.write("progress event\n");
      child.emit("close", 0, null);
    });
    return child;
  };

  const result = await executeCodexJob(job, {
    spawnImpl,
    createWorkingDirectory: () => "/tmp/codex-job",
    removeWorkingDirectory: (path) => { removed.push(path); },
  });

  assert.equal(invocation.options.cwd, "/tmp/codex-job");
  assert.deepEqual(removed, ["/tmp/codex-job"]);
  assert.deepEqual(result, {
    exitCode: 0,
    signal: null,
    stdout: "Use a state machine.\n",
    stderr: "progress event\n",
    error: null,
  });
});

test("Codex invocation applies and validates model options", () => {
  const invocation = buildCodexInvocation({
    ...job,
    modelOptions: { effort: "xhigh", timeoutMs: 900_000 },
  }, { cwd: "/tmp/codex-test" });

  assert.equal(
    invocation.args.includes("model_reasoning_effort=\"xhigh\""),
    true,
  );
  assert.throws(
    () => buildCodexInvocation({
      ...job,
      modelOptions: { effort: "unsupported" },
    }, { cwd: "/tmp/codex-test" }),
    /unsupported Codex effort/,
  );
  assert.throws(
    () => buildCodexInvocation({
      ...job,
      modelOptions: { unexpected: true },
    }, { cwd: "/tmp/codex-test" }),
    /unsupported Codex option/,
  );
});

test("Codex execution records spawn errors once and cleans up", async () => {
  let cleanupCount = 0;
  const spawnImpl = () => {
    const child = fakeChild();
    process.nextTick(() => {
      child.emit("error", new Error("spawn failed"));
      child.emit("close", 1, null);
    });
    return child;
  };

  const result = await executeCodexJob(job, {
    spawnImpl,
    createWorkingDirectory: () => "/tmp/codex-job",
    removeWorkingDirectory: () => { cleanupCount++; },
  });

  assert.equal(cleanupCount, 1);
  assert.equal(result.exitCode, null);
  assert.equal(result.error, "spawn failed");
});

test("Codex execution terminates timed-out processes", async () => {
  let killedWith;
  const spawnImpl = () => {
    const child = fakeChild();
    child.kill = (signal) => {
      killedWith = signal;
      return true;
    };
    return child;
  };

  const result = await executeCodexJob(job, {
    spawnImpl,
    timeoutMs: 5,
    createWorkingDirectory: () => "/tmp/codex-job",
    removeWorkingDirectory: () => {},
  });

  assert.equal(killedWith, "SIGTERM");
  assert.equal(result.signal, "SIGTERM");
  assert.match(result.error, /timed out after 5 ms/);
});

test("provider registry exposes the Codex adapter", () => {
  assert.equal(getProvider("codex"), executeCodexJob);
});

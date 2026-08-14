import { EventEmitter } from "node:events";
import { PassThrough } from "node:stream";
import test from "node:test";
import assert from "node:assert/strict";
import { extractAnswer } from "../src/answers.mjs";
import {
  buildClaudeCodeInvocation,
  executeClaudeCodeJob,
} from "../src/providers/claude-code.mjs";
import { getProvider } from "../src/providers/index.mjs";

const job = {
  run: 1,
  task: {
    id: "task-one",
    category: "test",
    prompt: "Write a robust parser.",
  },
  provider: "claude-code",
  modelName: "claude-sonnet-5",
  modelId: "claude-sonnet-5",
  modelOptions: {},
};

function fakeChild() {
  const child = new EventEmitter();
  child.stdout = new PassThrough();
  child.stderr = new PassThrough();
  child.kill = () => true;
  return child;
}

test("Claude Code invocation isolates the prompt and disables tools", () => {
  const invocation = buildClaudeCodeInvocation(job, {
    cwd: "/tmp/claude-code-test",
    environment: { PATH: "/bin" },
  });

  assert.equal(invocation.command, "claude");
  assert.deepEqual(invocation.options, {
    cwd: "/tmp/claude-code-test",
    env: {
      PATH: "/bin",
      CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC: "1",
      CLAUDE_CODE_SKIP_PROMPT_HISTORY: "1",
      ENABLE_CLAUDEAI_MCP_SERVERS: "false",
    },
    stdio: ["ignore", "pipe", "pipe"],
  });
  assert.deepEqual(invocation.args, [
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
    "claude-sonnet-5",
    "--effort",
    "medium",
    "--output-format",
    "json",
    "Write a robust parser.",
  ]);
});

test("Claude Code execution captures JSON and removes its workspace", async () => {
  let invocation;
  const removed = [];
  const spawnImpl = (command, args, options) => {
    invocation = { command, args, options };
    const child = fakeChild();
    process.nextTick(() => {
      child.stdout.write('{"type":"result","result":"Use a state machine."}');
      child.stderr.write("diagnostic\n");
      child.emit("close", 0, null);
    });
    return child;
  };

  const result = await executeClaudeCodeJob(job, {
    spawnImpl,
    createWorkingDirectory: () => "/tmp/claude-code-job",
    removeWorkingDirectory: (path) => { removed.push(path); },
    environment: {},
  });

  assert.equal(invocation.options.cwd, "/tmp/claude-code-job");
  assert.deepEqual(removed, ["/tmp/claude-code-job"]);
  assert.deepEqual(result, {
    exitCode: 0,
    signal: null,
    stdout: '{"type":"result","result":"Use a state machine."}',
    stderr: "diagnostic\n",
    error: null,
  });
  assert.equal(extractAnswer(result.stdout), "Use a state machine.");
});

test("Claude Code invocation applies and validates model options", () => {
  const invocation = buildClaudeCodeInvocation({
    ...job,
    modelOptions: { effort: "xhigh", timeoutMs: 900_000 },
  }, { cwd: "/tmp/claude-code-test" });

  assert.equal(invocation.args.includes("xhigh"), true);
  assert.throws(
    () => buildClaudeCodeInvocation({
      ...job,
      modelOptions: { effort: "unsupported" },
    }, { cwd: "/tmp/claude-code-test" }),
    /unsupported Claude Code effort/,
  );
  assert.throws(
    () => buildClaudeCodeInvocation({
      ...job,
      modelOptions: { unexpected: true },
    }, { cwd: "/tmp/claude-code-test" }),
    /unsupported Claude Code option/,
  );
});

test("Claude Code execution records spawn errors once and cleans up", async () => {
  let cleanupCount = 0;
  const spawnImpl = () => {
    const child = fakeChild();
    process.nextTick(() => {
      child.emit("error", new Error("spawn failed"));
      child.emit("close", 1, null);
    });
    return child;
  };

  const result = await executeClaudeCodeJob(job, {
    spawnImpl,
    createWorkingDirectory: () => "/tmp/claude-code-job",
    removeWorkingDirectory: () => { cleanupCount++; },
  });

  assert.equal(cleanupCount, 1);
  assert.equal(result.exitCode, null);
  assert.equal(result.error, "spawn failed");
});

test("Claude Code execution terminates timed-out processes", async () => {
  let killedWith;
  const spawnImpl = () => {
    const child = fakeChild();
    child.kill = (signal) => {
      killedWith = signal;
      return true;
    };
    return child;
  };

  const result = await executeClaudeCodeJob(job, {
    spawnImpl,
    timeoutMs: 5,
    createWorkingDirectory: () => "/tmp/claude-code-job",
    removeWorkingDirectory: () => {},
  });

  assert.equal(killedWith, "SIGTERM");
  assert.equal(result.signal, "SIGTERM");
  assert.match(result.error, /timed out after 5 ms/);
});

test("provider registry exposes the Claude Code adapter", () => {
  assert.equal(getProvider("claude-code"), executeClaudeCodeJob);
});

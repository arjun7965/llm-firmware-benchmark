import { EventEmitter } from "node:events";
import { PassThrough } from "node:stream";
import test from "node:test";
import assert from "node:assert/strict";
import { extractAnswer } from "../src/answers.mjs";
import {
  buildOpenCodeInvocation,
  executeOpenCodeJob,
} from "../src/providers/opencode.mjs";
import { getProvider } from "../src/providers/index.mjs";

const job = {
  run: 1,
  task: {
    id: "task-one",
    category: "test",
    prompt: "Write a robust parser.",
  },
  provider: "opencode",
  modelName: "example-model",
  modelId: "provider/example-model",
  modelOptions: {},
};

function fakeChild() {
  const child = new EventEmitter();
  child.stdout = new PassThrough();
  child.stderr = new PassThrough();
  child.kill = () => true;
  return child;
}

function event(type, part = undefined, sessionID = "ses_test") {
  return JSON.stringify({
    type,
    timestamp: 1_777_000_000_000,
    sessionID,
    ...(part === undefined ? {} : { part }),
  });
}

test("OpenCode invocation isolates configuration and disables tools", () => {
  const invocation = buildOpenCodeInvocation(job, {
    cwd: "/tmp/opencode-test",
    environment: {
      PATH: "/bin",
      OPENCODE_CONFIG: "/tmp/user-config.json",
      OPENCODE_PERMISSION: '{"*":"allow"}',
    },
  });

  assert.equal(invocation.command, "opencode");
  assert.deepEqual(invocation.args, [
    "--pure",
    "run",
    "--format",
    "json",
    "--model",
    "provider/example-model",
    "--agent",
    "benchmark",
    "--",
    "Write a robust parser.",
  ]);
  assert.deepEqual(invocation.options.stdio, ["ignore", "pipe", "pipe"]);
  assert.equal(invocation.options.cwd, "/tmp/opencode-test");
  assert.equal(invocation.options.env.PATH, "/bin");
  assert.equal(invocation.options.env.OPENCODE_CONFIG, undefined);
  assert.equal(invocation.options.env.OPENCODE_PERMISSION, undefined);
  assert.equal(
    invocation.options.env.OPENCODE_CONFIG_DIR,
    "/tmp/opencode-test/.opencode",
  );
  assert.equal(
    invocation.options.env.OPENCODE_DISABLE_GLOBAL_CONFIG,
    "true",
  );
  assert.equal(
    invocation.options.env.OPENCODE_DISABLE_PROJECT_CONFIG,
    "true",
  );
  const config = JSON.parse(
    invocation.options.env.OPENCODE_CONFIG_CONTENT,
  );
  assert.equal(config.share, "disabled");
  assert.equal(config.agent.benchmark.mode, "primary");
  assert.deepEqual(config.agent.benchmark.permission, { "*": "deny" });
});

test("OpenCode execution captures NDJSON and removes its workspace", async () => {
  let invocation;
  const removed = [];
  const stdout = [
    event("step_start", { type: "step-start" }),
    event("text", { type: "text", text: "Use a state machine." }),
    event("step_finish", { type: "step-finish" }),
  ].join("\n") + "\n";
  const spawnImpl = (command, args, options) => {
    invocation = { command, args, options };
    const child = fakeChild();
    process.nextTick(() => {
      child.stdout.write(stdout);
      child.stderr.write("diagnostic\n");
      child.emit("close", 0, null);
    });
    return child;
  };

  const result = await executeOpenCodeJob(job, {
    spawnImpl,
    createWorkingDirectory: () => "/tmp/opencode-job",
    removeWorkingDirectory: (path) => { removed.push(path); },
    environment: {},
  });

  assert.equal(invocation.options.cwd, "/tmp/opencode-job");
  assert.deepEqual(removed, ["/tmp/opencode-job"]);
  assert.deepEqual(result, {
    exitCode: 0,
    signal: null,
    stdout,
    stderr: "diagnostic\n",
    error: null,
  });
  assert.equal(extractAnswer(result.stdout), "Use a state machine.");
});

test("OpenCode answer extraction combines completed text parts", () => {
  const stdout = [
    event("step_start", { type: "step-start" }),
    event("text", { type: "text", text: "first" }),
    event("text", { type: "text", text: " second" }),
    event("step_finish", { type: "step-finish" }),
  ].join("\n");

  assert.equal(extractAnswer(stdout), "first second");
});

test("OpenCode answer extraction rejects invalid event streams", () => {
  const noText = [
    event("step_start", { type: "step-start" }),
    event("step_finish", { type: "step-finish" }),
  ].join("\n");
  const mixedSessions = [
    event("step_start", { type: "step-start" }),
    event("text", { type: "text", text: "answer" }, "ses_other"),
  ].join("\n");

  assert.throws(() => extractAnswer(noText), /does not contain a text result/);
  assert.throws(() => extractAnswer(mixedSessions), /multiple sessions/);
  assert.equal(extractAnswer('{"ordinary":true}\nnot-json'),
    '{"ordinary":true}\nnot-json');
});

test("OpenCode invocation applies and validates model options", () => {
  const invocation = buildOpenCodeInvocation({
    ...job,
    modelOptions: { timeoutMs: 900_000, variant: "high" },
  }, { cwd: "/tmp/opencode-test" });

  assert.deepEqual(invocation.args.slice(-4), [
    "--variant",
    "high",
    "--",
    "Write a robust parser.",
  ]);
  assert.throws(
    () => buildOpenCodeInvocation({
      ...job,
      modelOptions: { variant: "not valid" },
    }, { cwd: "/tmp/opencode-test" }),
    /OpenCode variant/,
  );
  assert.throws(
    () => buildOpenCodeInvocation({
      ...job,
      modelOptions: { unexpected: true },
    }, { cwd: "/tmp/opencode-test" }),
    /unsupported OpenCode option/,
  );
  assert.throws(
    () => buildOpenCodeInvocation({
      ...job,
      modelId: "missing-provider",
    }, { cwd: "/tmp/opencode-test" }),
    /provider\/model format/,
  );
});

test("OpenCode execution records spawn errors once and cleans up", async () => {
  let cleanupCount = 0;
  const spawnImpl = () => {
    const child = fakeChild();
    process.nextTick(() => {
      child.emit("error", new Error("spawn failed"));
      child.emit("close", 1, null);
    });
    return child;
  };

  const result = await executeOpenCodeJob(job, {
    spawnImpl,
    createWorkingDirectory: () => "/tmp/opencode-job",
    removeWorkingDirectory: () => { cleanupCount++; },
  });

  assert.equal(cleanupCount, 1);
  assert.equal(result.exitCode, null);
  assert.equal(result.error, "spawn failed");
});

test("OpenCode execution terminates timed-out processes", async () => {
  let killedWith;
  const spawnImpl = () => {
    const child = fakeChild();
    child.kill = (signal) => {
      killedWith = signal;
      return true;
    };
    return child;
  };

  const result = await executeOpenCodeJob(job, {
    spawnImpl,
    timeoutMs: 5,
    createWorkingDirectory: () => "/tmp/opencode-job",
    removeWorkingDirectory: () => {},
  });

  assert.equal(killedWith, "SIGTERM");
  assert.equal(result.signal, "SIGTERM");
  assert.match(result.error, /timed out after 5 ms/);
});

test("provider registry exposes the OpenCode adapter", () => {
  assert.equal(getProvider("opencode"), executeOpenCodeJob);
});

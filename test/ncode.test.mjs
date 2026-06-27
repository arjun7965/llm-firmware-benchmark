import { EventEmitter } from "node:events";
import { PassThrough } from "node:stream";
import test from "node:test";
import assert from "node:assert/strict";
import {
  buildNcodeInvocation,
  executeNcodeJob,
} from "../src/providers/ncode.mjs";

const job = {
  run: 1,
  task: {
    id: "task-one",
    category: "test",
    prompt: "First prompt",
  },
  provider: "ncode",
  modelName: "alpha",
  modelId: "model-a",
  modelOptions: {},
};

function fakeChild() {
  const child = new EventEmitter();
  child.stdout = new PassThrough();
  child.stderr = new PassThrough();
  child.kill = () => true;
  return child;
}

test("NCode invocation contains the required CLI arguments", () => {
  assert.deepEqual(buildNcodeInvocation(job), {
    command: "ncode",
    args: [
      "--print",
      "--no-session-persistence",
      "--tools",
      "",
      "--effort",
      "medium",
      "--model",
      "model-a",
      "--output-format",
      "json",
      "First prompt",
    ],
    options: {
      cwd: "/tmp",
      stdio: ["ignore", "pipe", "pipe"],
    },
  });
});

test("NCode execution captures output and process status", async () => {
  let invocation;
  const spawnImpl = (command, args, options) => {
    invocation = { command, args, options };
    const child = fakeChild();
    process.nextTick(() => {
      child.stdout.write("generated answer");
      child.stderr.write("diagnostic");
      child.emit("close", 0, null);
    });
    return child;
  };

  const result = await executeNcodeJob(job, { spawnImpl });

  assert.equal(invocation.command, "ncode");
  assert.deepEqual(
    invocation.args.slice(-3),
    ["--output-format", "json", "First prompt"],
  );
  assert.deepEqual(result, {
    exitCode: 0,
    signal: null,
    stdout: "generated answer",
    stderr: "diagnostic",
    error: null,
  });
});

test("NCode invocation applies validated model options", () => {
  const invocation = buildNcodeInvocation({
    ...job,
    modelOptions: { effort: "high" },
  });

  assert.equal(invocation.args[invocation.args.indexOf("--effort") + 1], "high");
  assert.throws(
    () => buildNcodeInvocation({
      ...job,
      modelOptions: { effort: "unsupported" },
    }),
    /unsupported NCode effort/,
  );
});

test("NCode execution records spawn errors once when close follows", async () => {
  const spawnImpl = () => {
    const child = fakeChild();
    process.nextTick(() => {
      child.emit("error", new Error("spawn failed"));
      child.emit("close", 1, null);
    });
    return child;
  };

  const result = await executeNcodeJob(job, { spawnImpl });

  assert.equal(result.exitCode, null);
  assert.equal(result.error, "spawn failed");
});

test("NCode execution terminates timed-out processes", async () => {
  let killedWith;
  const spawnImpl = () => {
    const child = fakeChild();
    child.kill = (signal) => {
      killedWith = signal;
      return true;
    };
    return child;
  };

  const result = await executeNcodeJob(job, {
    spawnImpl,
    timeoutMs: 5,
  });

  assert.equal(killedWith, "SIGTERM");
  assert.equal(result.signal, "SIGTERM");
  assert.match(result.error, /timed out after 5 ms/);
});

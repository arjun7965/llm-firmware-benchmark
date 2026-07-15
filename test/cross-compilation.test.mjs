import assert from "node:assert/strict";
import { test } from "node:test";

import {
  crossCompilationTargets,
  parseCrossCompilationArgs,
  runCrossCompilation,
} from "../src/cross-compilation.mjs";

test("cross-compilation targets define representative bare-metal ABIs", () => {
  assert.deepEqual(
    crossCompilationTargets.map((target) => target.id),
    ["armv7m-bare-metal", "rv32-bare-metal"],
  );
  assert.ok(crossCompilationTargets[0].flags.includes("-mthumb"));
  assert.ok(crossCompilationTargets[1].flags.includes("-mabi=ilp32"));
});

test("cross-compilation CLI selects targets and rejects unknown input", () => {
  assert.deepEqual(
    parseCrossCompilationArgs([
      "--require-tools",
      "--target",
      "rv32-bare-metal",
    ]),
    {
      help: false,
      requireTools: true,
      targetIds: ["rv32-bare-metal"],
    },
  );
  assert.throws(
    () => parseCrossCompilationArgs(["--target", "unknown"]),
    /Unknown cross-compilation target/u,
  );
});

test("missing optional cross-compilers are skipped or required", () => {
  const missingCompiler = () => ({
    error: Object.assign(new Error("missing"), { code: "ENOENT" }),
  });
  const summary = runCrossCompilation({
    spawn: missingCompiler,
    log: () => {},
  });
  assert.deepEqual(summary.compiledTargets, []);
  assert.deepEqual(summary.skippedTargets, [
    "armv7m-bare-metal",
    "rv32-bare-metal",
  ]);
  assert.throws(
    () => runCrossCompilation({
      requireTools: true,
      spawn: missingCompiler,
      log: () => {},
    }),
    /install gcc-arm-none-eabi/u,
  );
});

test("cross-compilation uses fixed argv without linking or execution", () => {
  const calls = [];
  const fakeCompiler = (command, args, options) => {
    calls.push({ command, args, options });
    return {
      status: 0,
      stdout: args[0] === "--version" ? "cross-gcc 1.0\n" : "",
      stderr: "",
    };
  };
  const summary = runCrossCompilation({
    targetIds: ["armv7m-bare-metal"],
    spawn: fakeCompiler,
    log: () => {},
  });

  assert.equal(summary.objectCount, 8);
  assert.equal(calls.length, 9);
  for (const call of calls.slice(1)) {
    assert.equal(call.command, "arm-none-eabi-gcc");
    assert.ok(call.args.includes("-c"));
    assert.ok(!call.args.includes("-o") || call.args.at(-1).endsWith(".o"));
    assert.equal(call.options.cwd, process.cwd());
  }

  calls.length = 0;
  const rvSummary = runCrossCompilation({
    targetIds: ["rv32-bare-metal"],
    spawn: fakeCompiler,
    log: () => {},
  });
  assert.equal(rvSummary.objectCount, 3);
  assert.equal(calls.length, 4);
  assert.ok(calls.slice(1).every((call) =>
    !call.args.includes(
      "fixtures/bare-metal-timer/reference/fictional_timer.c",
    )));
});

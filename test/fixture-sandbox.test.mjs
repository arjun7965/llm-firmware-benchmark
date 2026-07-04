import assert from "node:assert/strict";
import {
  chmodSync,
  mkdirSync,
  mkdtempSync,
  readFileSync,
  rmSync,
  symlinkSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import {
  basename,
  join,
} from "node:path";
import test from "node:test";
import {
  buildSandboxInvocation,
  resolveExecutable,
  runFixtureValidation,
  validateFixtureValidationReport,
} from "../src/fixture-sandbox.mjs";

function temporaryDirectory(t) {
  const path = mkdtempSync(join(tmpdir(), "fixture-sandbox-test-"));
  t.after(() => rmSync(path, { recursive: true, force: true }));
  return path;
}

function sandboxFixture(t) {
  const root = temporaryDirectory(t);
  const fixturesRoot = join(root, "fixtures");
  const fixtureRoot = join(fixturesRoot, "example-task");
  for (const directory of [
    "generated",
    "mocks",
    "reference",
    "scripts",
    "starter",
    "tests/public",
  ]) {
    mkdirSync(join(fixtureRoot, directory), { recursive: true });
  }
  writeFileSync(
    join(fixtureRoot, "generated", "answer.c"),
    "int answer(void) { return 0; }\n",
  );
  const tasksPath = join(root, "tasks.json");
  writeFileSync(tasksPath, JSON.stringify([{
    id: "example-task",
    category: "systems-security",
    suite: "firmware",
    targetProfile: "portable-c11",
    prompt: "Return code.",
  }]));
  const manifest = {
    schemaVersion: "1.2",
    taskId: "example-task",
    targetProfile: "portable-c11",
    status: "active",
    language: "c11",
    toolVersionArgs: {
      cc: ["version"],
    },
    answer: {
      format: "markdown-fenced-code",
      language: "c",
      output: "generated/answer.c",
    },
    paths: {
      starter: "starter",
      mocks: "mocks",
      publicTests: "tests/public",
      reference: "reference",
      scripts: "scripts",
      generated: "generated",
      build: "build",
    },
    commands: [
      {
        id: "host-compile",
        phase: "compile",
        argv: ["cc", "-c", "generated/answer.c", "-o", "build/tests"],
        requiredTools: ["cc"],
        timeoutMs: 30_000,
      },
      {
        id: "public-tests",
        phase: "test",
        argv: ["build/tests"],
        requiredTools: [],
        timeoutMs: 5_000,
      },
    ],
  };
  writeFileSync(
    join(fixtureRoot, "manifest.json"),
    JSON.stringify(manifest),
  );
  return {
    fixtureRoot,
    fixturesRoot,
    manifest,
    root,
    tasksPath,
  };
}

function fakeExecutable(name) {
  return `/usr/bin/${name}`;
}

function fakeToolVersion(command, args) {
  assert.deepEqual(args, ["version"]);
  return {
    status: 0,
    signal: null,
    stdout: `${basename(command)} test-version\n`,
    stderr: "",
  };
}

function deterministicNow() {
  let milliseconds = Date.parse("2026-01-01T00:00:00.000Z");
  return () => {
    const value = new Date(milliseconds);
    milliseconds += 10;
    return value;
  };
}

test("sandbox invocation isolates files, network, and build permissions", (t) => {
  const fixture = sandboxFixture(t);
  const buildRoot = temporaryDirectory(t);
  const compile = buildSandboxInvocation({
    bubblewrapPath: "/usr/bin/bwrap",
    prlimitPath: "/usr/bin/prlimit",
    fixtureRoot: fixture.fixtureRoot,
    manifest: fixture.manifest,
    buildRoot,
    command: fixture.manifest.commands[0],
    toolPath: "/usr/bin/cc",
    systemMounts: ["/usr", "/lib"],
  });
  const testRun = buildSandboxInvocation({
    bubblewrapPath: "/usr/bin/bwrap",
    prlimitPath: "/usr/bin/prlimit",
    fixtureRoot: fixture.fixtureRoot,
    manifest: fixture.manifest,
    buildRoot,
    command: fixture.manifest.commands[1],
    systemMounts: ["/lib", "/lib64"],
  });

  assert.equal(compile.command, "/usr/bin/prlimit");
  assert.ok(compile.args.includes("--unshare-all"));
  assert.ok(compile.args.includes("--disable-userns"));
  assert.equal(compile.args.includes("--share-net"), false);
  assert.ok(compile.args.includes("/usr/bin/cc"));
  const rootTmpfs = compile.args.indexOf("/");
  assert.deepEqual(
    compile.args.slice(rootTmpfs - 3, rootTmpfs + 1),
    ["--size", String(32 * 1024 * 1024), "--tmpfs", "/"],
  );
  assert.deepEqual(
    compile.args.slice(compile.args.indexOf(buildRoot) - 1,
      compile.args.indexOf(buildRoot) + 2),
    ["--bind", buildRoot, "/workspace/build"],
  );
  const rootRemount = compile.args.indexOf("--remount-ro");
  assert.deepEqual(
    compile.args.slice(rootRemount, rootRemount + 2),
    ["--remount-ro", "/"],
  );
  assert.ok(rootRemount > compile.args.indexOf(buildRoot));
  assert.deepEqual(
    testRun.args.slice(testRun.args.indexOf(buildRoot) - 1,
      testRun.args.indexOf(buildRoot) + 2),
    ["--ro-bind", buildRoot, "/workspace/build"],
  );
  assert.equal(testRun.args.at(-1), "build/tests");
  assert.equal(testRun.args.includes("/usr"), false);

  const userTools = temporaryDirectory(t);
  const fakeTool = join(userTools, "bwrap");
  writeFileSync(fakeTool, "");
  chmodSync(fakeTool, 0o700);
  assert.throws(
    () => resolveExecutable("bwrap", { pathValue: userTools }),
    /required executable not found/u,
  );
});

test("sandbox validation records successful compile and test phases", (t) => {
  const fixture = sandboxFixture(t);
  const calls = [];
  const { report, reportPath } = runFixtureValidation({
    taskId: "example-task",
    fixturesRoot: fixture.fixturesRoot,
    tasksPath: fixture.tasksPath,
    resolveExecutableImpl: fakeExecutable,
    spawnTool: fakeToolVersion,
    now: deterministicNow(),
    spawn: (command, args, options) => {
      calls.push({ command, args, options });
      const buildDestination = args.findIndex(
        (value, index) =>
          value === "/workspace/build" &&
          args[index - 2] === "--bind",
      );
      if (calls.length === 1 && buildDestination > 0) {
        const executable = join(args[buildDestination - 1], "tests");
        writeFileSync(executable, "");
        chmodSync(executable, 0o700);
      }
      return {
        status: 0,
        signal: null,
        stdout: "ok\n",
        stderr: "",
      };
    },
  });

  assert.equal(report.success, true);
  assert.equal(report.schemaVersion, "1.2");
  assert.equal(report.suite, "firmware");
  assert.equal(report.language, "c11");
  assert.match(report.answerSha256, /^[a-f0-9]{64}$/u);
  assert.deepEqual(report.toolchains, [{
    name: "cc",
    executable: "/usr/bin/cc",
    version: "cc test-version",
    versionArgv: ["/usr/bin/cc", "version"],
  }]);
  assert.deepEqual(report.artifacts, [{
    path: "build/tests",
    sizeBytes: 0,
  }]);
  assert.deepEqual(
    report.phases.map((phase) => phase.phase),
    ["compile", "test"],
  );
  assert.deepEqual(
    report.phases.map((phase) => phase.outcome),
    ["passed", "passed"],
  );
  assert.deepEqual(
    report.phases[0].argv,
    ["cc", "-c", "generated/answer.c", "-o", "build/tests"],
  );
  assert.equal(calls.length, 2);
  assert.equal(validateFixtureValidationReport(report), report);
  assert.throws(
    () => validateFixtureValidationReport({
      ...report,
      suite: "auxiliary",
    }),
    /auxiliary fixture validation.*targetProfile/u,
  );
  assert.throws(
    () => validateFixtureValidationReport({
      ...report,
      phases: [
        {
          ...report.phases[0],
          outcome: "failed",
        },
        report.phases[1],
      ],
    }),
    /phase outcome is inconsistent/u,
  );
  assert.deepEqual(
    JSON.parse(readFileSync(reportPath, "utf8")),
    report,
  );
});

test("sandbox validation stops after compilation failure", (t) => {
  const fixture = sandboxFixture(t);
  let calls = 0;
  const { report } = runFixtureValidation({
    taskId: "example-task",
    fixturesRoot: fixture.fixturesRoot,
    tasksPath: fixture.tasksPath,
    resolveExecutableImpl: fakeExecutable,
    spawnTool: fakeToolVersion,
    now: deterministicNow(),
    spawn: () => {
      calls++;
      return {
        status: 1,
        signal: null,
        stdout: "",
        stderr: "compile failed",
      };
    },
  });

  assert.equal(calls, 1);
  assert.equal(report.success, false);
  assert.deepEqual(report.artifacts, []);
  assert.equal(report.phases.length, 1);
  assert.equal(report.phases[0].outcome, "failed");
  assert.equal(report.phases[0].stderr, "compile failed");

  const timedOutFixture = sandboxFixture(t);
  const { report: timedOutReport } = runFixtureValidation({
    taskId: "example-task",
    fixturesRoot: timedOutFixture.fixturesRoot,
    tasksPath: timedOutFixture.tasksPath,
    resolveExecutableImpl: fakeExecutable,
    spawnTool: fakeToolVersion,
    now: deterministicNow(),
    spawn: () => ({
      status: null,
      signal: "SIGKILL",
      stdout: "",
      stderr: "",
      error: Object.assign(new Error("timed out"), {
        code: "ETIMEDOUT",
      }),
    }),
  });
  assert.equal(timedOutReport.success, false);
  assert.equal(timedOutReport.phases[0].outcome, "timed-out");
  assert.equal(timedOutReport.phases[0].timedOut, true);
});

test("sandbox validation rejects missing or symlinked answers", (t) => {
  const missing = sandboxFixture(t);
  rmSync(join(missing.fixtureRoot, "generated", "answer.c"));
  assert.throws(
    () => runFixtureValidation({
      taskId: "example-task",
      fixturesRoot: missing.fixturesRoot,
      tasksPath: missing.tasksPath,
      resolveExecutableImpl: fakeExecutable,
    }),
    /extracted answer does not exist/u,
  );

  const linked = sandboxFixture(t);
  const answerPath = join(linked.fixtureRoot, "generated", "answer.c");
  rmSync(answerPath);
  const outside = join(linked.root, "outside.c");
  writeFileSync(outside, "int outside;\n");
  symlinkSync(outside, answerPath);
  assert.throws(
    () => runFixtureValidation({
      taskId: "example-task",
      fixturesRoot: linked.fixturesRoot,
      tasksPath: linked.tasksPath,
      resolveExecutableImpl: fakeExecutable,
    }),
    /non-symlink regular file/u,
  );
});

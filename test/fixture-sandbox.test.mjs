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
  parseOsRelease,
  readValidationHost,
  resolveExecutable,
  runFixtureValidation,
  validateFixtureValidationReport,
} from "../src/fixture-sandbox.mjs";
import {
  getValidationEnvironmentRevision,
  getValidationProfile,
  profileFingerprint,
  validationEnvironmentReference,
} from "../src/validation-profiles.mjs";

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
    validationProfile: "c11-host",
    prompt: "Return code.",
  }]));
  const manifest = {
    schemaVersion: "1.3",
    taskId: "example-task",
    targetProfile: "portable-c11",
    validationProfile: "c11-host",
    status: "active",
    language: "c11",
    toolVersionArgs: {
      cc: ["--version"],
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
  assert.deepEqual(args, ["--version"]);
  const versions = {
    bwrap: "bubblewrap 0.9.0",
    cc: "cc (Ubuntu) 13.3.0",
    prlimit: "prlimit from util-linux 2.39.3",
  };
  return {
    status: 0,
    signal: null,
    stdout: `${versions[basename(command)]}\n`,
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

function matchingValidationHost() {
  return {
    operatingSystem: "ubuntu",
    release: "24.04",
    architecture: "x86_64",
  };
}

test("validation host detection parses os-release without executing it", (t) => {
  const root = temporaryDirectory(t);
  const osReleasePath = join(root, "os-release");
  writeFileSync(
    osReleasePath,
    [
      "NAME=\"Ubuntu\"",
      "ID=ubuntu",
      "VERSION_ID=\"24.04\"",
      "",
    ].join("\n"),
  );
  assert.deepEqual(
    readValidationHost({
      architecture: "x64",
      osReleasePath,
    }),
    matchingValidationHost(),
  );
  assert.deepEqual(
    parseOsRelease("ID='debian'\nVERSION_ID=13\n"),
    new Map([
      ["ID", "debian"],
      ["VERSION_ID", "13"],
    ]),
  );
  assert.throws(
    () => parseOsRelease("ID=ubuntu\nID=debian\n"),
    /duplicate ID/u,
  );
});

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

  const goCompile = buildSandboxInvocation({
    bubblewrapPath: "/usr/bin/bwrap",
    prlimitPath: "/usr/bin/prlimit",
    fixtureRoot: fixture.fixtureRoot,
    manifest: {
      ...fixture.manifest,
      validationProfile: "go-std",
    },
    buildRoot,
    command: {
      ...fixture.manifest.commands[0],
      argv: ["go", "run", "starter/build_tests.go"],
      requiredTools: ["go"],
    },
    toolPath: "/usr/bin/go",
    systemMounts: [],
  });
  const environmentValue = (name) => {
    const index = goCompile.args.indexOf(name);
    assert.ok(index > 0, `missing sandbox environment variable ${name}`);
    assert.equal(goCompile.args[index - 1], "--setenv");
    return goCompile.args[index + 1];
  };
  assert.equal(environmentValue("GOCACHE"), "/workspace/build/go-cache");
  assert.equal(
    environmentValue("GOMODCACHE"),
    "/workspace/build/go-mod-cache",
  );
  assert.equal(environmentValue("GOTOOLCHAIN"), "local");
  assert.equal(environmentValue("GOWORK"), "off");
  assert.equal(environmentValue("GOENV"), "off");
  assert.equal(environmentValue("CGO_ENABLED"), "0");
  assert.equal(environmentValue("FIXTURE_GO_EXECUTABLE"), "/usr/bin/go");

  const userTools = temporaryDirectory(t);
  const fakeTool = join(userTools, "bwrap");
  writeFileSync(fakeTool, "");
  chmodSync(fakeTool, 0o700);
  assert.throws(
    () => resolveExecutable("bwrap", { pathValue: userTools }),
    /required executable not found/u,
  );
});

test("sandbox rejects profiles with unavailable dependencies or runtimes", (t) => {
  const fixture = sandboxFixture(t);
  const tasks = JSON.parse(readFileSync(fixture.tasksPath, "utf8"));
  tasks[0].suite = "auxiliary";
  tasks[0].validationProfile = "node-typescript";
  delete tasks[0].targetProfile;
  writeFileSync(fixture.tasksPath, JSON.stringify(tasks));

  const manifest = {
    ...fixture.manifest,
    targetProfile: null,
    validationProfile: "node-typescript",
    status: "scaffold",
    toolVersionArgs: {
      node: ["--version"],
      tsc: ["--version"],
    },
    commands: [
      {
        ...fixture.manifest.commands[0],
        argv: ["tsc", "--noEmit"],
        requiredTools: ["node", "tsc"],
      },
      fixture.manifest.commands[1],
    ],
  };
  writeFileSync(
    join(fixture.fixtureRoot, "manifest.json"),
    JSON.stringify(manifest),
  );

  assert.throws(
    () => runFixtureValidation({
      taskId: "example-task",
      fixturesRoot: fixture.fixturesRoot,
      tasksPath: fixture.tasksPath,
    }),
    /dependency installation cannot be runtime-attested/u,
  );

  const interpreterFixture = sandboxFixture(t);
  const interpreterTasks = JSON.parse(
    readFileSync(interpreterFixture.tasksPath, "utf8"),
  );
  interpreterTasks[0].suite = "auxiliary";
  interpreterTasks[0].validationProfile = "python3-stdlib";
  delete interpreterTasks[0].targetProfile;
  writeFileSync(
    interpreterFixture.tasksPath,
    JSON.stringify(interpreterTasks),
  );
  writeFileSync(
    join(interpreterFixture.fixtureRoot, "manifest.json"),
    JSON.stringify({
      ...interpreterFixture.manifest,
      targetProfile: null,
      validationProfile: "python3-stdlib",
      status: "scaffold",
      toolVersionArgs: {
        python3: ["--version"],
      },
      commands: [
        {
          ...interpreterFixture.manifest.commands[0],
          argv: ["python3", "-m", "py_compile", "generated/answer.c"],
          requiredTools: ["python3"],
        },
        {
          ...interpreterFixture.manifest.commands[1],
          argv: [
            "python3",
            "-m",
            "unittest",
            "tests/public/test_pool.py",
          ],
          requiredTools: ["python3"],
        },
      ],
    }),
  );
  assert.throws(
    () => runFixtureValidation({
      taskId: "example-task",
      fixturesRoot: interpreterFixture.fixturesRoot,
      tasksPath: interpreterFixture.tasksPath,
    }),
    /test runtime is not mounted/u,
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
    readValidationHostImpl: matchingValidationHost,
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
  assert.equal(report.schemaVersion, "1.5");
  assert.equal(report.suite, "firmware");
  assert.equal(report.validationProfile, "c11-host");
  assert.equal(report.validationProfileRevision, 2);
  assert.match(report.validationProfileSha256, /^[a-f0-9]{64}$/u);
  assert.deepEqual(report.validationEnvironment.host, {
    operatingSystem: "ubuntu",
    release: "24.04",
    architecture: "x86_64",
  });
  assert.equal(
    report.validationEnvironment.id,
    "ubuntu-24-04-x86-64-c11-host",
  );
  assert.equal(report.validationEnvironment.revision, 1);
  assert.match(report.validationEnvironment.sha256, /^[a-f0-9]{64}$/u);
  assert.deepEqual(report.validationEnvironment.execution, { kind: "host" });
  assert.equal(report.language, "c11");
  assert.match(report.answerSha256, /^[a-f0-9]{64}$/u);
  assert.deepEqual(report.toolchains, [{
    name: "cc",
    executable: "/usr/bin/cc",
    version: "cc (Ubuntu) 13.3.0",
    versionArgv: ["/usr/bin/cc", "--version"],
  }]);
  assert.equal(report.sandbox.runtimeVersion, "bubblewrap 0.9.0");
  assert.equal(
    report.sandbox.limiterVersion,
    "prlimit from util-linux 2.39.3",
  );
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
      validationProfile: "unknown-profile",
    }),
    /validationProfile/u,
  );
  assert.throws(
    () => validateFixtureValidationReport({
      ...report,
      validationProfileRevision: 3,
    }),
    /validationProfileRevision/u,
  );
  assert.throws(
    () => validateFixtureValidationReport({
      ...report,
      validationEnvironment: {
        ...report.validationEnvironment,
        sha256: "0".repeat(64),
      },
    }),
    /validationEnvironmentSha256/u,
  );
  assert.throws(
    () => validateFixtureValidationReport({
      ...report,
      validationEnvironment: {
        ...report.validationEnvironment,
        host: {
          ...report.validationEnvironment.host,
          release: "22.04",
        },
      },
    }),
    /does not match environment/u,
  );
  assert.throws(
    () => validateFixtureValidationReport({
      ...report,
      sandbox: {
        ...report.sandbox,
        runtimeVersion: "bubblewrap 1.0.0",
      },
    }),
    /sandbox versions do not match profile/u,
  );
  assert.throws(
    () => validateFixtureValidationReport({
      ...report,
      sandbox: {
        ...report.sandbox,
        runtimeVersion: ["bubblewrap 0.9.0"],
      },
    }),
    /sandbox versions are invalid/u,
  );
  assert.throws(
    () => validateFixtureValidationReport({
      ...report,
      sandbox: {
        ...report.sandbox,
        limiterVersion: "prlimit 2.39.3\0",
      },
    }),
    /sandbox versions are invalid/u,
  );
  assert.throws(
    () => validateFixtureValidationReport({
      ...report,
      sandbox: {
        ...report.sandbox,
        runtimeVersion: "bubblewrap 0.9.0-beta.1",
      },
    }),
    /sandbox versions do not match profile/u,
  );
  assert.throws(
    () => validateFixtureValidationReport({
      ...report,
      sandbox: {
        ...report.sandbox,
        resourceLimits: {
          ...report.sandbox.resourceLimits,
          test: {
            ...report.sandbox.resourceLimits.test,
            cpuSeconds: 4,
          },
        },
      },
    }),
    /does not match profile/u,
  );
  assert.throws(
    () => validateFixtureValidationReport({
      ...report,
      toolchains: [{
        ...report.toolchains[0],
        version: "cc 13.3.0-beta.1",
      }],
    }),
    /toolchain does not match profile/u,
  );
  assert.throws(
    () => validateFixtureValidationReport({
      ...report,
      toolchains: [{
        ...report.toolchains[0],
        version: "cc 13.3.0rc1",
      }],
    }),
    /toolchain does not match profile/u,
  );
  assert.throws(
    () => validateFixtureValidationReport({
      ...report,
      toolchains: [{
        ...report.toolchains[0],
        versionArgv: ["/usr/bin/cc", "--help"],
      }],
    }),
    /versionArgv does not match profile/u,
  );
  const stableRustProfile = getValidationProfile("stable-rust");
  const stableRustEnvironment = getValidationEnvironmentRevision(
    stableRustProfile.environments[0].id,
    stableRustProfile.environments[0].revision,
  );
  assert.throws(
    () => validateFixtureValidationReport({
      ...report,
      validationProfile: stableRustProfile.id,
      validationProfileRevision: stableRustProfile.revision,
      validationProfileSha256: profileFingerprint(stableRustProfile),
      validationEnvironment: {
        ...validationEnvironmentReference(stableRustEnvironment),
        host: stableRustEnvironment.host,
        execution: stableRustEnvironment.execution,
      },
      sandbox: {
        ...report.sandbox,
        resourceLimits: stableRustProfile.sandbox.resourceLimits,
      },
      toolchains: [{
        name: "rustc",
        executable: "/usr/bin/rustc",
        version: "rustc 1.87.0",
        versionArgv: ["/usr/bin/rustc", "--version"],
      }],
    }),
    /toolchains do not cover profile exactly/u,
  );
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

  const mismatched = sandboxFixture(t);
  assert.throws(
    () => runFixtureValidation({
      taskId: "example-task",
      fixturesRoot: mismatched.fixturesRoot,
      tasksPath: mismatched.tasksPath,
      resolveExecutableImpl: fakeExecutable,
      readValidationHostImpl: matchingValidationHost,
      spawnTool: (command, args) => {
        const result = fakeToolVersion(command, args);
        return basename(command) === "cc"
          ? { ...result, stdout: "cc (Ubuntu) 14.2.0\n" }
          : result;
      },
    }),
    /cc version does not match validation profile/u,
  );

  const wrongHost = sandboxFixture(t);
  assert.throws(
    () => runFixtureValidation({
      taskId: "example-task",
      fixturesRoot: wrongHost.fixturesRoot,
      tasksPath: wrongHost.tasksPath,
      readValidationHostImpl: () => ({
        operatingSystem: "debian",
        release: "13",
        architecture: "x86_64",
      }),
      resolveExecutableImpl: fakeExecutable,
    }),
    /validation host does not match exactly one supported environment/u,
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
    readValidationHostImpl: matchingValidationHost,
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
    readValidationHostImpl: matchingValidationHost,
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
      readValidationHostImpl: matchingValidationHost,
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
      readValidationHostImpl: matchingValidationHost,
    }),
    /non-symlink regular file/u,
  );
});

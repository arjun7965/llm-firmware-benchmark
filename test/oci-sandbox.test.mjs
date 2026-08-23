import assert from "node:assert/strict";
import {
  existsSync,
  mkdtempSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import {
  buildOciInvocation,
  inspectOciImage,
  inspectOciRuntime,
  inspectOciToolchain,
  runOciInvocation,
} from "../src/oci-sandbox.mjs";

const digest = `sha256:${"a".repeat(64)}`;
const imageReference = `ghcr.io/example/validator@${digest}`;
const source = "https://github.com/example/validator";
const revision = "b".repeat(40);
const execution = Object.freeze({
  kind: "oci",
  image: imageReference,
  source,
  revision,
});
const manifest = Object.freeze({ paths: { build: "build" } });
const profile = Object.freeze({
  environment: {
    host: {
      architecture: "x86_64",
      operatingSystem: "debian",
      release: "13",
    },
  },
  sandbox: {
    rootTmpfsBytes: 8 * 1024 * 1024,
    temporaryDirectoryBytes: 16 * 1024 * 1024,
    resourceLimits: {
      compile: {
        addressSpaceBytes: 512 * 1024 * 1024,
        cpuSeconds: 15,
        fileBytes: 16 * 1024 * 1024,
        openFiles: 64,
      },
      test: {
        addressSpaceBytes: 128 * 1024 * 1024,
        cpuSeconds: 3,
        fileBytes: 1024 * 1024,
        openFiles: 32,
      },
    },
  },
});

function temporaryDirectory(t) {
  const path = mkdtempSync(join(tmpdir(), "oci-sandbox-test-"));
  t.after(() => rmSync(path, { recursive: true, force: true }));
  return path;
}

function successfulResult(stdout = "") {
  return {
    status: 0,
    signal: null,
    stdout,
    stderr: "",
  };
}

function podmanInformation(overrides = {}) {
  return {
    host: {
      arch: "amd64",
      cgroupVersion: "v2",
      serviceIsRemote: false,
      security: {
        rootless: true,
        seccompEnabled: true,
      },
      ...overrides,
    },
    plugins: { log: ["journald", "none"] },
  };
}

function inspectedImage(overrides = {}) {
  return {
    Architecture: "amd64",
    Digest: digest,
    Id: "c".repeat(64),
    Labels: {
      "org.opencontainers.image.revision": revision,
      "org.opencontainers.image.source": source,
    },
    Os: "linux",
    RepoDigests: [imageReference],
    ...overrides,
  };
}

test("OCI runtime inspection requires local rootless Podman security", () => {
  const calls = [];
  const spawn = (command, args, options) => {
    calls.push({ command, args, options });
    return args[0] === "info"
      ? successfulResult(JSON.stringify(podmanInformation()))
      : successfulResult("podman version 5.4.2\n");
  };
  const runtime = inspectOciRuntime({
    runtimePath: "/usr/bin/podman",
    expectedVersion: "5.4.2",
    versionArgs: ["--version"],
    spawn,
    environment: { LANG: "C" },
    userId: 1000,
  });
  assert.deepEqual(runtime, {
    architecture: "x86_64",
    executable: "/usr/bin/podman",
    name: "podman",
    rootless: true,
    version: "podman version 5.4.2",
    versionArgv: ["/usr/bin/podman", "--version"],
  });
  assert.deepEqual(calls.map((call) => call.args), [
    ["--version"],
    ["info", "--format=json"],
  ]);
  assert.equal(calls[0].options.env.LANG, "C");

  for (const insecureInfo of [
    podmanInformation({ security: {
      rootless: false,
      seccompEnabled: true,
    } }),
    podmanInformation({ serviceIsRemote: true }),
    podmanInformation({ cgroupVersion: "v1" }),
    podmanInformation({ security: {
      rootless: true,
      seccompEnabled: false,
    } }),
  ]) {
    assert.throws(
      () => inspectOciRuntime({
        runtimePath: "/usr/bin/podman",
        expectedVersion: "5.4.2",
        versionArgs: ["--version"],
        userId: 1000,
        spawn: (_command, args) => args[0] === "info"
          ? successfulResult(JSON.stringify(insecureInfo))
          : successfulResult("podman version 5.4.2\n"),
      }),
      /must be local, rootless, seccomp-enabled, cgroup v2/u,
    );
  }
  assert.throws(
    () => inspectOciRuntime({
      runtimePath: "/usr/bin/podman",
      expectedVersion: "5.4.2",
      versionArgs: ["--version"],
      userId: 0,
      spawn: () => assert.fail("root must be rejected before execution"),
    }),
    /requires a non-root host user/u,
  );
});

test("OCI image inspection verifies the local digest, platform, and labels", () => {
  const calls = [];
  const metadata = inspectOciImage({
    runtimePath: "/usr/bin/podman",
    execution,
    expectedArchitecture: "x86_64",
    spawn: (command, args) => {
      calls.push({ command, args });
      return successfulResult(JSON.stringify([inspectedImage()]));
    },
    environment: { LANG: "C" },
  });
  assert.deepEqual(metadata, {
    architecture: "x86_64",
    digest,
    id: `sha256:${"c".repeat(64)}`,
    operatingSystem: "linux",
    reference: imageReference,
    revision,
    source,
  });
  assert.deepEqual(calls, [{
    command: "/usr/bin/podman",
    args: ["image", "inspect", imageReference],
  }]);

  for (const [image, error] of [
    [inspectedImage({ Digest: `sha256:${"d".repeat(64)}`, RepoDigests: [] }),
      /digest does not match/u],
    [inspectedImage({ Architecture: "arm64" }), /platform does not match/u],
    [inspectedImage({ Os: "windows" }), /platform does not match/u],
    [inspectedImage({ Labels: {
      "org.opencontainers.image.revision": "d".repeat(40),
      "org.opencontainers.image.source": source,
    } }), /provenance does not match/u],
  ]) {
    assert.throws(
      () => inspectOciImage({
        runtimePath: "/usr/bin/podman",
        execution,
        expectedArchitecture: "x86_64",
        spawn: () => successfulResult(JSON.stringify([image])),
      }),
      error,
    );
  }
  for (const unsafeReference of [
    "ghcr.io/example/validator:latest",
    `--rootfs@${digest}`,
    `ghcr.io/example/../validator@${digest}`,
  ]) {
    assert.throws(
      () => inspectOciImage({
        runtimePath: "/usr/bin/podman",
        execution: { ...execution, image: unsafeReference },
        expectedArchitecture: "x86_64",
        spawn: () => assert.fail(
          "unsafe references must be rejected before inspection",
        ),
      }),
      /must be digest-pinned/u,
    );
  }
});

test("OCI invocations enforce isolation, limits, and read-only test inputs", (t) => {
  const root = temporaryDirectory(t);
  const inputRoot = join(root, "input");
  const buildRoot = join(root, "build");
  const cidFile = join(root, "compile.cid");
  const compile = buildOciInvocation({
    runtimePath: "/usr/bin/podman",
    execution,
    inputRoot,
    buildRoot,
    manifest,
    profile,
    command: {
      id: "compile",
      phase: "compile",
      argv: ["cc", "generated/answer.c", "-o", "build/tests"],
      requiredTools: ["cc"],
      timeoutMs: 30_000,
    },
    environment: {
      PATH: "/usr/local/bin:/usr/bin:/bin",
      HOME: "/nonexistent",
    },
    cidFile,
  });
  for (const argument of [
    "--events-backend=none",
    "--pull=never",
    "--network=none",
    "--ipc=none",
    "--pid=private",
    "--cgroupns=private",
    "--userns=keep-id:uid=65532,gid=65532",
    "--user=65532:65532",
    "--cap-drop=all",
    "--security-opt=no-new-privileges",
    "--read-only",
    "--read-only-tmpfs=false",
    "--image-volume=ignore",
    "--no-healthcheck",
    "--no-hosts",
    "--http-proxy=false",
    "--env-host=false",
    "--unsetenv-all",
    "--log-driver=none",
    "--entrypoint=",
    "--stop-timeout=1",
    "--systemd=false",
    "--umask=077",
    "--memory=536870912b",
    "--memory-swap=536870913b",
    "--pids-limit=256",
    "--ulimit=as=536870912:536870912",
    "--ulimit=cpu=15:15",
    "--ulimit=fsize=16777216:16777216",
    "--ulimit=nofile=64:64",
    "--ulimit=core=0:0",
  ]) {
    assert.ok(compile.args.includes(argument), `missing ${argument}`);
  }
  assert.equal(compile.command, "/usr/bin/podman");
  assert.equal(compile.options.timeout, 30_000);
  assert.equal(compile.args.includes("--privileged"), false);
  assert.ok(compile.args.includes(
    "--tmpfs=/tmp:rw,noexec,nosuid,nodev,size=16777216",
  ));
  assert.ok(compile.args.includes(
    "--tmpfs=/run:rw,noexec,nosuid,nodev,size=8388608",
  ));
  assert.ok(compile.args.includes(
    `type=bind,src=${inputRoot},dst=/workspace,ro=true,` +
      "relabel=shared,bind-nonrecursive",
  ));
  assert.ok(compile.args.includes(
    `type=bind,src=${buildRoot},dst=/workspace/build,ro=false,` +
      "relabel=private,bind-nonrecursive",
  ));
  assert.ok(
    compile.args.indexOf("--env=HOME=/nonexistent") <
      compile.args.indexOf("--env=PATH=/usr/local/bin:/usr/bin:/bin"),
  );
  assert.deepEqual(
    compile.args.slice(compile.args.indexOf(imageReference)),
    [imageReference, "cc", "generated/answer.c", "-o", "build/tests"],
  );

  const testInvocation = buildOciInvocation({
    runtimePath: "/usr/bin/podman",
    execution,
    inputRoot,
    buildRoot,
    manifest,
    profile,
    command: {
      id: "test",
      phase: "test",
      argv: ["build/tests"],
      requiredTools: [],
      timeoutMs: 5_000,
    },
    environment: { PATH: "/usr/bin:/bin" },
    cidFile: join(root, "test.cid"),
  });
  assert.ok(testInvocation.args.includes(
    `type=bind,src=${buildRoot},dst=/workspace/build,ro=true,` +
      "relabel=private,bind-nonrecursive",
  ));
  assert.ok(testInvocation.args.includes("--memory=134217728b"));

  assert.throws(
    () => buildOciInvocation({
      runtimePath: "/usr/bin/podman",
      execution: { ...execution, image: "ghcr.io/example/validator:latest" },
      inputRoot,
      buildRoot,
      manifest,
      profile,
      command: { phase: "test", argv: ["true"], timeoutMs: 1000 },
      environment: {},
      cidFile: join(root, "tagged.cid"),
    }),
    /must be digest-pinned/u,
  );
});

test("OCI invocation cleanup force-removes a recorded container", (t) => {
  const root = temporaryDirectory(t);
  const cidFile = join(root, "candidate.cid");
  const containerId = "d".repeat(64);
  writeFileSync(cidFile, `${containerId}\n`);
  const cleanupCalls = [];
  const timeoutError = Object.assign(new Error("timed out"), {
    code: "ETIMEDOUT",
  });
  const result = runOciInvocation({
    command: "/usr/bin/podman",
    args: ["run"],
    cidFile,
    options: {},
  }, {
    spawn: () => ({
      error: timeoutError,
      status: null,
      signal: "SIGKILL",
      stdout: "",
      stderr: "",
    }),
    cleanupSpawn: (command, args) => {
      cleanupCalls.push({ command, args });
      return successfulResult();
    },
    environment: { LANG: "C" },
  });
  assert.equal(result.error, timeoutError);
  assert.deepEqual(cleanupCalls, [{
    command: "/usr/bin/podman",
    args: [
      "--events-backend=none",
      "rm",
      "--force",
      "--time=0",
      "--ignore",
      containerId,
    ],
  }]);
  assert.equal(existsSync(cidFile), false);

  const invalidCidFile = join(root, "invalid.cid");
  writeFileSync(invalidCidFile, "../../unexpected\n");
  assert.throws(
    () => runOciInvocation({
      command: "/usr/bin/podman",
      args: ["run"],
      cidFile: invalidCidFile,
      options: {},
    }, {
      spawn: () => successfulResult(),
      cleanupSpawn: () => assert.fail("invalid IDs must not reach Podman"),
    }),
    /invalid container ID/u,
  );
  assert.equal(existsSync(invalidCidFile), false);
});

test("OCI toolchain probes execute inside the pinned sandbox", (t) => {
  const root = temporaryDirectory(t);
  const calls = [];
  const toolchain = inspectOciToolchain({
    runtimePath: "/usr/bin/podman",
    execution,
    inputRoot: join(root, "input"),
    buildRoot: join(root, "build"),
    manifest,
    profile,
    name: "cc",
    versionArgs: ["--version"],
    expectedVersion: "14.2.0",
    environment: { PATH: "/usr/bin:/bin" },
    cidFile: join(root, "probe.cid"),
    spawn: (command, args) => {
      calls.push({ command, args });
      return successfulResult("cc (Debian 14.2.0) 14.2.0\n");
    },
  });
  assert.deepEqual(toolchain, {
    executable: "oci:cc",
    name: "cc",
    version: "cc (Debian 14.2.0) 14.2.0",
    versionArgv: ["cc", "--version"],
  });
  assert.equal(calls.length, 1);
  assert.deepEqual(calls[0].args.slice(-3), [
    imageReference,
    "cc",
    "--version",
  ]);
  assert.ok(calls[0].args.includes("--pull=never"));
  assert.ok(calls[0].args.includes("--network=none"));
});

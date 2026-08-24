import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import {
  chmodSync,
  existsSync,
  mkdtempSync,
  readFileSync,
  rmSync,
  statSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import {
  buildOciInvocation,
  cleanupOciContainer,
  createOciRuntimeEnvironment,
  inspectOciImage,
  inspectOciRuntime,
  inspectOciSeccompProfile,
  inspectOciToolchain,
  ociRuntimeConfigurationFingerprint,
  removeOciStateRoot,
  runOciInvocation,
} from "../src/oci-sandbox.mjs";

const digest = `sha256:${"a".repeat(64)}`;
const imageReference = `ghcr.io/example/validator@${digest}`;
const source = "https://github.com/example/validator";
const revision = "b".repeat(40);
const containerRuntime = Object.freeze({
  executable: "/usr/bin/crun",
  name: "crun",
  version: "1.20.0",
  versionArgs: ["--version"],
});
const monitor = Object.freeze({
  executable: "/usr/bin/conmon",
  name: "conmon",
  version: "2.1.12",
  versionArgs: ["--version"],
});
const seccompProfile = Object.freeze({
  path: "/usr/share/containers/seccomp.json",
  sha256: "d".repeat(64),
});
const runtimeConfigurationSha256 = ociRuntimeConfigurationFingerprint({
  containerRuntime,
  monitor,
});
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
      cgroupManager: "systemd",
      cgroupVersion: "v2",
      conmon: {
        path: monitor.executable,
        version: `conmon version ${monitor.version}`,
      },
      eventLogger: "none",
      logDriver: "none",
      ociRuntime: {
        name: containerRuntime.name,
        path: containerRuntime.executable,
        version: `crun version ${containerRuntime.version}`,
      },
      serviceIsRemote: false,
      security: {
        rootless: true,
        seccompEnabled: true,
        seccompProfilePath: seccompProfile.path,
      },
      ...overrides,
    },
    plugins: { log: ["journald", "none"] },
  };
}

function runtimeInspectionSpawn(information, calls = null) {
  return (command, args, options) => {
    calls?.push({ command, args, options });
    if (command === containerRuntime.executable) {
      return successfulResult(`crun version ${containerRuntime.version}\n`);
    }
    if (command === monitor.executable) {
      return successfulResult(`conmon version ${monitor.version}\n`);
    }
    return args[0] === "info"
      ? successfulResult(JSON.stringify(information))
      : successfulResult("podman version 5.4.2\n");
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
  let seccompInspection;
  const runtime = inspectOciRuntime({
    runtimePath: "/usr/bin/podman",
    expectedVersion: "5.4.2",
    versionArgs: ["--version"],
    containerRuntime,
    monitor,
    expectedSeccompSha256: seccompProfile.sha256,
    spawn: runtimeInspectionSpawn(podmanInformation(), calls),
    environment: { LANG: "C" },
    userId: 1000,
    inspectSeccompProfile: (options) => {
      seccompInspection = options;
      return seccompProfile;
    },
  });
  assert.deepEqual(runtime, {
    architecture: "x86_64",
    executable: "/usr/bin/podman",
    name: "podman",
    containerRuntime: {
      executable: containerRuntime.executable,
      name: containerRuntime.name,
      version: `crun version ${containerRuntime.version}`,
      versionArgv: [containerRuntime.executable, "--version"],
    },
    monitor: {
      executable: monitor.executable,
      name: monitor.name,
      version: `conmon version ${monitor.version}`,
      versionArgv: [monitor.executable, "--version"],
    },
    rootless: true,
    seccompProfile,
    version: "podman version 5.4.2",
    versionArgv: ["/usr/bin/podman", "--version"],
  });
  assert.deepEqual(calls.map((call) => call.args), [
    ["--version"],
    ["--version"],
    ["--version"],
    ["info", "--format=json"],
  ]);
  assert.ok(calls.every((call) => call.options.env.LANG === "C"));
  assert.deepEqual(seccompInspection, {
    expectedSha256: seccompProfile.sha256,
    path: seccompProfile.path,
  });

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
        containerRuntime,
        monitor,
        expectedSeccompSha256: seccompProfile.sha256,
        userId: 1000,
        inspectSeccompProfile: () => seccompProfile,
        spawn: runtimeInspectionSpawn(insecureInfo),
      }),
      /must be local, rootless, seccomp-enabled, cgroup v2/u,
    );
  }
  assert.throws(
    () => inspectOciRuntime({
      runtimePath: "/usr/bin/podman",
      expectedVersion: "5.4.2",
      versionArgs: ["--version"],
      containerRuntime,
      monitor,
      expectedSeccompSha256: seccompProfile.sha256,
      userId: 0,
      spawn: () => assert.fail("root must be rejected before execution"),
    }),
    /requires a non-root host user/u,
  );
  for (const [information, error] of [
    [podmanInformation({
      ociRuntime: {
        name: "runc",
        path: "/usr/bin/runc",
        version: "runc version 1.2.0",
      },
    }), /container runtime does not match/u],
    [podmanInformation({
      conmon: {
        path: "/usr/local/bin/conmon",
        version: `conmon version ${monitor.version}`,
      },
    }), /monitor does not match/u],
  ]) {
    assert.throws(
      () => inspectOciRuntime({
        runtimePath: "/usr/bin/podman",
        expectedVersion: "5.4.2",
        versionArgs: ["--version"],
        containerRuntime,
        monitor,
        expectedSeccompSha256: seccompProfile.sha256,
        userId: 1000,
        inspectSeccompProfile: () => seccompProfile,
        spawn: runtimeInspectionSpawn(information),
      }),
      error,
    );
  }
});

test("OCI runtime configuration excludes ambient Podman overrides", (t) => {
  const root = temporaryDirectory(t);
  const prepared = createOciRuntimeEnvironment({
    stateRoot: root,
    containerRuntime,
    monitor,
    expectedConfigurationSha256: runtimeConfigurationSha256,
    environment: {
      CONTAINER_HOST: "ssh://unexpected.example",
      CONTAINERS_CONF_MODULES: "/tmp/untrusted.conf",
      CONTAINERS_CONF_OVERRIDE: "/tmp/override.conf",
      HOME: "/srv/validator",
      LANG: "en_US.UTF-8",
      PATH: "/tmp/untrusted:/usr/bin",
      PODMAN_CONNECTIONS_CONF: "/tmp/connections.json",
      XDG_DATA_HOME: "/srv/validator/data",
      XDG_RUNTIME_DIR: "/run/user/1000",
    },
  });
  assert.equal(prepared.configurationSha256, runtimeConfigurationSha256);
  assert.equal(
    prepared.environment.CONTAINERS_CONF,
    join(root, "containers.conf"),
  );
  assert.equal(prepared.environment.XDG_CONFIG_HOME, join(root, "xdg-config"));
  assert.equal(prepared.environment.PATH, "/usr/bin:/bin");
  assert.equal(prepared.environment.LANG, "C");
  assert.equal(prepared.environment.LC_ALL, "C");
  assert.equal(prepared.environment.TMPDIR, root);
  assert.equal(prepared.environment.HOME, "/srv/validator");
  for (const name of [
    "CONTAINER_HOST",
    "CONTAINERS_CONF_MODULES",
    "CONTAINERS_CONF_OVERRIDE",
    "PODMAN_CONNECTIONS_CONF",
  ]) {
    assert.equal(Object.hasOwn(prepared.environment, name), false);
  }
  assert.equal(statSync(prepared.configurationPath).mode & 0o777, 0o600);
  const configuration = readFileSync(prepared.configurationPath, "utf8");
  for (const line of [
    "default_capabilities = []",
    "devices = []",
    "env_host = false",
    "privileged = false",
    "read_only = true",
    "seccomp_profile = \"/usr/share/containers/seccomp.json\"",
    "cdi_spec_dirs = []",
    "cgroup_manager = \"systemd\"",
    "conmon_env_vars = []",
    "hooks_dir = []",
    "pull_policy = \"never\"",
    "remote = false",
    "runtime = \"crun\"",
    "conmon_path = [\"/usr/bin/conmon\"]",
    "crun = [\"/usr/bin/crun\"]",
  ]) {
    assert.ok(configuration.includes(line), `missing ${line}`);
  }

  const mismatchedRoot = temporaryDirectory(t);
  assert.throws(
    () => createOciRuntimeEnvironment({
      stateRoot: mismatchedRoot,
      containerRuntime,
      monitor,
      expectedConfigurationSha256: "e".repeat(64),
      environment: {},
    }),
    /configuration fingerprint does not match/u,
  );
  assert.equal(existsSync(join(mismatchedRoot, "containers.conf")), false);

  const sharedRoot = temporaryDirectory(t);
  chmodSync(sharedRoot, 0o755);
  assert.throws(
    () => createOciRuntimeEnvironment({
      stateRoot: sharedRoot,
      containerRuntime,
      monitor,
      expectedConfigurationSha256: runtimeConfigurationSha256,
      environment: {},
    }),
    /must be a private directory/u,
  );
});

test("OCI security profiles must match a trusted system file", (t) => {
  const systemFile = "/usr/bin/true";
  const sha256 = createHash("sha256")
    .update(readFileSync(systemFile))
    .digest("hex");
  assert.deepEqual(inspectOciSeccompProfile({
    expectedSha256: sha256,
    path: systemFile,
  }), { path: systemFile, sha256 });
  assert.throws(
    () => inspectOciSeccompProfile({
      expectedSha256: "0".repeat(64),
      path: systemFile,
    }),
    /fingerprint does not match/u,
  );

  const untrustedFile = join(temporaryDirectory(t), "seccomp.json");
  writeFileSync(untrustedFile, "{}\n");
  assert.throws(
    () => inspectOciSeccompProfile({
      expectedSha256: createHash("sha256")
        .update(readFileSync(untrustedFile))
        .digest("hex"),
      path: untrustedFile,
    }),
    /not a trusted system file/u,
  );
});

test("OCI image inspection verifies the local digest, platform, and labels", () => {
  const calls = [];
  const metadata = inspectOciImage({
    runtimePath: "/usr/bin/podman",
    execution,
    expectedArchitecture: "x86_64",
    spawn: (command, args, options) => {
      calls.push({ command, args, options });
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
  assert.equal(calls[0].command, "/usr/bin/podman");
  assert.deepEqual(calls[0].args, ["image", "inspect", imageReference]);
  assert.deepEqual(calls[0].options.env, { LANG: "C" });

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
    "--uts=private",
    "--cgroupns=private",
    "--userns=keep-id:uid=65532,gid=65532",
    "--user=65532:65532",
    "--cap-drop=all",
    "--privileged=false",
    "--security-opt=no-new-privileges",
    "--security-opt=seccomp=/usr/share/containers/seccomp.json",
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
    "--ulimit=cpu=15:15",
    "--ulimit=fsize=16777216:16777216",
    "--ulimit=nofile=64:64",
    "--ulimit=core=0:0",
  ]) {
    assert.ok(compile.args.includes(argument), `missing ${argument}`);
  }
  assert.equal(
    compile.args.some((argument) => argument.startsWith("--ulimit=as=")),
    false,
  );
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
  let primaryOptions;
  const result = runOciInvocation({
    command: "/usr/bin/podman",
    args: ["run"],
    cidFile,
    options: {},
  }, {
    spawn: (_command, _args, options) => {
      primaryOptions = options;
      return {
        error: timeoutError,
        status: null,
        signal: "SIGKILL",
        stdout: "",
        stderr: "",
      };
    },
    cleanupSpawn: (command, args) => {
      cleanupCalls.push({ command, args });
      return successfulResult();
    },
    environment: { LANG: "C" },
  });
  assert.equal(result.error, timeoutError);
  assert.deepEqual(primaryOptions.env, { LANG: "C" });
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
  assert.equal(existsSync(invalidCidFile), true);

  const failedCleanupCidFile = join(root, "failed-cleanup.cid");
  writeFileSync(failedCleanupCidFile, `${"e".repeat(64)}\n`);
  assert.throws(
    () => cleanupOciContainer({
      runtimePath: "/usr/bin/podman",
      cidFile: failedCleanupCidFile,
      spawn: () => ({
        status: 1,
        signal: null,
        stdout: "",
        stderr: "cleanup failed",
      }),
      environment: { LANG: "C" },
    }),
    new RegExp(`recovery ID remains at ${failedCleanupCidFile}`, "u"),
  );
  assert.equal(existsSync(failedCleanupCidFile), true);
});

test("OCI state roots are preserved while recovery IDs remain", (t) => {
  const preservedRoot = temporaryDirectory(t);
  writeFileSync(join(preservedRoot, "containers.conf"), "configuration\n");
  const cidFile = join(preservedRoot, "candidate.cid");
  writeFileSync(cidFile, `${"f".repeat(64)}\n`);
  assert.equal(removeOciStateRoot(preservedRoot), false);
  assert.equal(existsSync(preservedRoot), true);

  rmSync(cidFile);
  assert.equal(removeOciStateRoot(preservedRoot), true);
  assert.equal(existsSync(preservedRoot), false);
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
    runtimeEnvironment: { CONTAINERS_CONF: "/tmp/containers.conf" },
    cidFile: join(root, "probe.cid"),
    spawn: (command, args, options) => {
      calls.push({ command, args, options });
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
  assert.equal(calls[0].options.timeout, 30_000);
  assert.deepEqual(calls[0].options.env, {
    CONTAINERS_CONF: "/tmp/containers.conf",
  });

  assert.throws(
    () => inspectOciToolchain({
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
      cidFile: join(root, "failed-probe.cid"),
      spawn: () => ({
        status: 125,
        signal: null,
        stdout: "",
        stderr: "runtime rejected an isolation option\nsecond line\n",
      }),
    }),
    /failed: status 125; runtime rejected an isolation option second line/u,
  );
});

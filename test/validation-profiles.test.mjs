import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { createHash } from "node:crypto";
import test from "node:test";
import { ociRuntimeConfigurationFingerprint } from "../src/oci-sandbox.mjs";
import { loadOciImageActivation } from "../src/oci-image-recipe.mjs";
import {
  environmentFingerprint,
  getValidationProfile,
  getValidationEnvironmentRevision,
  getValidationProfileRevision,
  normalizeDependencyLockfileContent,
  profileFingerprint,
  resolveValidationProfile,
  sandboxProfileBlockReason,
  selectValidationEnvironment,
  selectValidationEnvironmentFrom,
  validateValidationProfileFingerprints,
  validateValidationEnvironmentReference,
  validateValidationProfileReference,
  validateValidationProfiles,
  validationEnvironmentIds,
  validationEnvironments,
  validationEnvironmentReference,
  validationProfileReference,
  validationProfileIds,
  validationProfiles,
  validationProfilesDocument,
  validateValidationProfileLockfiles,
} from "../src/validation-profiles.mjs";

const ociDigest = `sha256:${"a".repeat(64)}`;
const imageReference = `ghcr.io/example/validator@${ociDigest}`;
const ociSourceRevision = "b".repeat(40);
const ociContainerRuntime = Object.freeze({
  name: "crun",
  executable: "/usr/bin/crun",
  version: "1.20.0",
  versionArgs: ["--version"],
});
const ociMonitor = Object.freeze({
  name: "conmon",
  executable: "/usr/bin/conmon",
  version: "2.1.12",
  versionArgs: ["--version"],
});

function ociValidationDocument() {
  const environment = {
    id: "debian-13-x86-64-c11-oci",
    revision: 1,
    host: {
      operatingSystem: "debian",
      release: "13",
      architecture: "x86_64",
    },
    execution: {
      kind: "oci",
      image: imageReference,
      source: "https://github.com/example/validator",
      revision: ociSourceRevision,
    },
    sandbox: {
      configurationSha256: ociRuntimeConfigurationFingerprint({
        containerRuntime: ociContainerRuntime,
        monitor: ociMonitor,
      }),
      containerRuntime: structuredClone(ociContainerRuntime),
      runtime: {
        name: "podman",
        executable: "podman",
        version: "5.4.2",
        versionArgs: ["--version"],
      },
      limiter: {
        name: "podman",
        executable: "podman",
        version: "5.4.2",
        versionArgs: ["--version"],
      },
      monitor: structuredClone(ociMonitor),
      seccompProfile: {
        path: "/usr/share/containers/seccomp.json",
        sha256: "c".repeat(64),
      },
    },
    toolchains: [{
      name: "cc",
      version: "14.2.0",
      versionArgs: ["--version"],
    }],
  };
  const profile = {
    id: "oci-c11",
    revision: 1,
    dependencies: [],
    environments: [{ id: environment.id, revision: environment.revision }],
    toolchains: ["cc"],
    sandbox: structuredClone(getValidationProfile("c11-host").sandbox),
  };
  return {
    schemaVersion: "2.6",
    environments: [environment],
    profiles: [profile],
  };
}

test("hosted validation profiles are pinned and immutable", () => {
  const fingerprintsDocument = JSON.parse(
    readFileSync(
      new URL(
        "../validation-profile-fingerprints.json",
        import.meta.url,
      ),
      "utf8",
    ),
  );
  assert.equal(
    validateValidationProfiles(validationProfilesDocument),
    validationProfilesDocument,
  );
  assert.equal(
    validateValidationProfileLockfiles(
      validationProfilesDocument,
      new URL("../", import.meta.url),
    ),
    validationProfilesDocument,
  );
  assert.equal(
    validateValidationProfileFingerprints(
      validationProfilesDocument,
      fingerprintsDocument,
    ),
    fingerprintsDocument,
  );
  assert.deepEqual(
    [...new Set(validationProfiles.map((profile) => profile.id))],
    validationProfileIds,
  );
  assert.deepEqual(
    [...new Set(validationEnvironments.map((environment) => environment.id))],
    validationEnvironmentIds,
  );
  for (const profile of validationProfiles) {
    assert.ok(Number.isSafeInteger(profile.revision));
    assert.ok(profile.revision >= 1);
    assert.ok(profile.toolchains.length > 0);
    if (
      profile.dependencies.length > 0 &&
      getValidationProfile(profile.id) === profile
    ) {
      assert.equal(profile.dependencyInstall.kind, "lockfile");
      assert.match(profile.dependencyInstall.sha256, /^[a-f0-9]{64}$/u);
    }
    assert.match(profileFingerprint(profile), /^[a-f0-9]{64}$/u);
    assert.equal(Object.isFrozen(profile), true);
    assert.equal(Object.isFrozen(profile.sandbox.resourceLimits.test), true);
  }
  const environment = getValidationEnvironmentRevision(
    "ubuntu-24-04-x86-64-c11-host",
    1,
  );
  assert.equal(environment.host.release, "24.04");
  assert.match(environmentFingerprint(environment), /^[a-f0-9]{64}$/u);
  assert.equal(Object.isFrozen(environment), true);
  assert.deepEqual(
    validateValidationEnvironmentReference(
      validationEnvironmentReference(environment),
    ),
    environment,
  );
  const debianEnvironment = getValidationEnvironmentRevision(
    "debian-13-x86-64-c11-host",
    1,
  );
  assert.deepEqual(debianEnvironment.host, {
    operatingSystem: "debian",
    release: "13",
    architecture: "x86_64",
  });
  assert.deepEqual(
    selectValidationEnvironment(
      getValidationProfile("c11-host"),
      debianEnvironment.host,
    ),
    debianEnvironment,
  );
  assert.equal(getValidationProfile("c11-host").revision, 4);
  const stableEnvironment = getValidationEnvironmentRevision(
    "ubuntu-24-04-x86-64-stable-rust",
    2,
  );
  assert.equal(
    resolveValidationProfile(
      getValidationProfile("stable-rust"),
      stableEnvironment,
    ).toolchains
      .find((toolchain) => toolchain.name === "rustc").version,
    "1.87.0",
  );
  assert.equal(
    getValidationProfileRevision("stable-rust", 3),
    getValidationProfile("stable-rust"),
  );
  assert.deepEqual(
    getValidationProfile("stable-rust").toolchains,
    ["cargo", "cc", "rustc"],
  );
  assert.equal(getValidationProfile("go-std").revision, 5);
  assert.equal(
    getValidationProfile("go-std").sandbox.temporaryDirectoryBytes,
    256 * 1024 * 1024,
  );
  assert.equal(
    getValidationProfile("go-std").sandbox.resourceLimits.test
      .addressSpaceBytes,
    1024 * 1024 * 1024,
  );
  assert.equal(
    getValidationProfile("go-std").sandbox.resourceLimits.compile.openFiles,
    256,
  );
  assert.equal(
    getValidationProfile("python3-stdlib").revision,
    4,
  );
  assert.equal(getValidationProfile("node-typescript").revision, 4);
  assert.equal(getValidationProfile("node-typescript-postgresql").revision, 5);
  assert.equal(getValidationProfile("react18-typescript").revision, 4);
  assert.equal(
    getValidationProfile("node-typescript").sandbox.resourceLimits.compile
      .addressSpaceBytes,
    2 * 1024 * 1024 * 1024,
  );
  assert.equal(
    getValidationProfile("node-typescript").sandbox.resourceLimits.test
      .addressSpaceBytes,
    2 * 1024 * 1024 * 1024,
  );
  assert.equal(
    getValidationProfile("node-typescript").dependencyInstall.mountPath,
    "/workspace/node_modules",
  );
  assert.deepEqual(
    getValidationProfile("node-typescript").testRuntime.mounts.map((mount) =>
      mount.path),
    ["/usr/local/lib/node-22.16.0"],
  );
  assert.deepEqual(
    getValidationProfile("node-typescript-postgresql").toolchains,
    ["initdb", "node", "pg_ctl", "postgres", "psql"],
  );
  assert.equal(
    getValidationProfile("node-typescript-postgresql").dependencyInstall
      .mountPath,
    "/workspace/node_modules",
  );
  assert.equal(
    getValidationProfile("node-typescript-postgresql").dependencyInstall
      .installRoot,
    "/usr/local/lib/node-typescript-postgresql-5/node_modules",
  );
  assert.equal(
    getValidationProfile("node-typescript-postgresql").dependencyInstall
      .installSha256,
    "2cee5684bb6e504ccefa44d74596086cc280b1947cecd9c9ed4bfd7abdbe42ec",
  );
  assert.deepEqual(
    getValidationProfile("node-typescript-postgresql").testRuntime.mounts
      .map((mount) => mount.path),
    [
      "/usr/local/lib/node-22.16.0",
      "/usr/local/lib/node-typescript-postgresql-5/node_modules",
      "/usr/local/lib/postgresql-16.9",
    ],
  );
  assert.equal(
    getValidationProfile("node-typescript-postgresql").testRuntime.service
      .kind,
    "postgresql",
  );
  assert.deepEqual(
    getValidationProfile("node-typescript-postgresql").testRuntime
      .commandContracts.map((contract) => contract.id),
    [
      "typescript-compile",
      "webhook-typescript-compile",
      "node-postgresql-public-tests",
      "webhook-public-tests",
    ],
  );
  assert.equal(
    getValidationProfile("react18-typescript").dependencyInstall.mountPath,
    "/workspace/node_modules",
  );
  assert.equal(
    getValidationProfile("react18-typescript").sandbox.resourceLimits.compile
      .addressSpaceBytes,
    2 * 1024 * 1024 * 1024,
  );
  assert.equal(
    getValidationProfile("react18-typescript").sandbox.resourceLimits.test
      .addressSpaceBytes,
    2 * 1024 * 1024 * 1024,
  );
  assert.deepEqual(
    getValidationProfile("react18-typescript").testRuntime.mounts.map(
      (mount) => mount.path,
    ),
    [
      "/usr/local/lib/node-22.16.0",
      "/usr/local/lib/react18-typescript-4/node_modules",
    ],
  );
  assert.deepEqual(
    getValidationProfile("python3-stdlib").testRuntime.mounts.map((mount) =>
      mount.path),
    ["/usr/local/lib/python-3.12.11"],
  );
  assert.equal(
    getValidationProfile("postgresql").testRuntime.commandContracts[0].id,
    "postgresql-load",
  );
  assert.equal(
    getValidationProfile("postgresql").testRuntime.service.kind,
    "postgresql",
  );
  assert.match(
    getValidationProfile("postgresql").testRuntime.service.readyArgv.at(-1),
    /ALTER ROLE validator NOLOGIN/u,
  );
  assert.deepEqual(
    getValidationProfile("postgresql").toolchains,
    ["initdb", "pg_ctl", "postgres", "psql"],
  );
  assert.deepEqual(
    validateValidationProfileReference(
      validationProfileReference(getValidationProfile("stable-rust")),
    ),
    getValidationProfile("stable-rust"),
  );
  assert.deepEqual(
    selectValidationEnvironment(
      getValidationProfile("stable-rust"),
      environment.host,
    ),
    stableEnvironment,
  );
  assert.throws(
    () => getValidationProfile("unknown"),
    /validationProfile is invalid/u,
  );
  const changedProfiles = structuredClone(validationProfilesDocument);
  changedProfiles.profiles[0].sandbox.resourceLimits.test.cpuSeconds++;
  assert.throws(
    () => validateValidationProfileFingerprints(
      changedProfiles,
      fingerprintsDocument,
    ),
    /c11-host@1 fingerprint is invalid/u,
  );
  const changedLockfileHash = structuredClone(validationProfilesDocument);
  changedLockfileHash.profiles
    .find((profile) => profile.id === "node-typescript" &&
      profile.revision === 4)
    .dependencyInstall.sha256 = "0".repeat(64);
  assert.throws(
    () => validateValidationProfileLockfiles(
      changedLockfileHash,
      new URL("../", import.meta.url),
    ),
    /dependencyInstall sha256 does not match/u,
  );
  const lockfileContent = readFileSync(
    new URL(
      "../validation-locks/node-typescript-v4/package-lock.json",
      import.meta.url,
    ),
    "utf8",
  );
  const crlfContent = lockfileContent.replace(/\n/gu, "\r\n");
  assert.equal(
    createHash("sha256")
      .update(normalizeDependencyLockfileContent(crlfContent))
      .digest("hex"),
    getValidationProfile("node-typescript").dependencyInstall.sha256,
  );
});

test("validation profile contracts reject unpinned or unsafe values", () => {
  const validProfile = structuredClone(
    getValidationProfileRevision("c11-host", 1),
  );
  const secondRevision = {
    ...structuredClone(validProfile),
    revision: 2,
  };
  assert.throws(
    () => validateValidationProfiles({
      schemaVersion: "2.6",
      environments: structuredClone(validationEnvironments),
      profiles: [validProfile, secondRevision],
    }),
    /current validation profile c11-host@2 must use the logical/u,
  );
  assert.throws(
    () => validateValidationProfiles({
      schemaVersion: "2.6",
      environments: structuredClone(validationEnvironments),
      profiles: [{
        ...validProfile,
        toolchains: [{
          ...validProfile.toolchains[0],
          version: "latest",
        }],
      }],
    }),
    /toolchain version is invalid/u,
  );
  assert.throws(
    () => validateValidationProfiles({
      schemaVersion: "2.6",
      environments: structuredClone(validationEnvironments),
      profiles: [{
        ...validProfile,
        sandbox: {
          ...validProfile.sandbox,
          network: "host",
        },
      }],
    }),
    /sandbox policy is invalid/u,
  );
  assert.throws(
    () => validateValidationProfiles({
      schemaVersion: "2.6",
      environments: structuredClone(validationEnvironments),
      profiles: [validProfile, structuredClone(validProfile)],
    }),
    /sorted and contiguous/u,
  );
  assert.throws(
    () => validateValidationProfiles({
      schemaVersion: "2.6",
      environments: structuredClone(validationEnvironments),
      profiles: [{
        ...structuredClone(validProfile),
        revision: 2,
      }],
    }),
    /must start at 1/u,
  );
  const logicalProfile = structuredClone(getValidationProfile("c11-host"));
  assert.throws(
    () => validateValidationProfiles({
      schemaVersion: "2.6",
      environments: structuredClone(validationEnvironments),
      profiles: [{
        ...logicalProfile,
        environments: [{ id: "unknown-environment", revision: 1 }],
      }],
    }),
    /environment is unknown/u,
  );
  assert.throws(
    () => selectValidationEnvironment(logicalProfile, {
      operatingSystem: "fedora",
      release: "42",
      architecture: "x86_64",
    }),
    /does not match exactly one supported environment/u,
  );
  const mismatchedTools = structuredClone(validationProfilesDocument);
  const c11Environment = mismatchedTools.environments.find((environment) =>
    environment.id === "ubuntu-24-04-x86-64-c11-host");
  const goEnvironment = mismatchedTools.environments.find((environment) =>
    environment.id === "ubuntu-24-04-x86-64-go-std");
  c11Environment.toolchains.push(structuredClone(goEnvironment.toolchains[0]));
  assert.throws(
    () => validateValidationProfiles(mismatchedTools),
    /toolchains must exactly match environment/u,
  );
  const missingInstall = structuredClone(validationProfilesDocument);
  delete missingInstall.profiles
    .find((profile) => profile.id === "node-typescript" &&
      profile.revision === 4)
    .dependencyInstall;
  assert.throws(
    () => validateValidationProfiles(missingInstall),
    /current validation profile node-typescript@4 must define dependencyInstall/u,
  );
  const mismatchedInstall = structuredClone(validationProfilesDocument);
  mismatchedInstall.profiles
    .find((profile) => profile.id === "node-typescript" &&
      profile.revision === 4)
    .dependencyInstall.source = "pypi";
  assert.throws(
    () => validateValidationProfiles(mismatchedInstall),
    /dependencyInstall source does not cover/u,
  );
  const unsafeInstallRoot = structuredClone(validationProfilesDocument);
  unsafeInstallRoot.profiles
    .find((profile) => profile.id === "node-typescript" &&
      profile.revision === 4)
    .dependencyInstall.installRoot = "/opt/node-typescript";
  assert.throws(
    () => validateValidationProfiles(unsafeInstallRoot),
    /dependencyInstall installRoot is invalid/u,
  );
  const unsafeInstallMount = structuredClone(validationProfilesDocument);
  unsafeInstallMount.profiles
    .find((profile) => profile.id === "node-typescript" &&
      profile.revision === 4)
    .dependencyInstall.mountPath = "/etc/node_modules";
  assert.throws(
    () => validateValidationProfiles(unsafeInstallMount),
    /dependencyInstall mountPath is invalid/u,
  );
  assert.equal(
    getValidationProfile("python3-pytest-hypothesis")
      .dependencyInstall.source,
    "pypi",
  );
  const unsafeRuntimeMount = structuredClone(validationProfilesDocument);
  unsafeRuntimeMount.profiles
    .find((profile) => profile.id === "python3-stdlib" &&
      profile.revision === 3)
    .testRuntime.mounts[0].path = "/opt/python3";
  assert.throws(
    () => validateValidationProfiles(unsafeRuntimeMount),
    /testRuntime mount path is invalid/u,
  );
  const unsafeRuntimeCommand = structuredClone(validationProfilesDocument);
  unsafeRuntimeCommand.profiles
    .find((profile) => profile.id === "python3-stdlib" &&
      profile.revision === 3)
    .testRuntime.commandContracts[0].argvPrefix[0] = "sh";
  assert.throws(
    () => validateValidationProfiles(unsafeRuntimeCommand),
    /commandContract executable is invalid/u,
  );
  const unknownRuntimeTool = structuredClone(validationProfilesDocument);
  unknownRuntimeTool.profiles
    .find((profile) => profile.id === "python3-stdlib" &&
      profile.revision === 3)
    .testRuntime.commandContracts[0].requiredTools = ["pytest"];
  assert.throws(
    () => validateValidationProfiles(unknownRuntimeTool),
    /tool pytest is not in its profile/u,
  );
  const unsafeServiceCommand = structuredClone(validationProfilesDocument);
  unsafeServiceCommand.profiles
    .find((profile) => profile.id === "postgresql" && profile.revision === 4)
    .testRuntime.service.startArgv[0] = "sh";
  assert.throws(
    () => validateValidationProfiles(unsafeServiceCommand),
    /service startArgv is invalid/u,
  );
});

test("OCI validation environments are explicit, pinned, and platform-safe", () => {
  const document = ociValidationDocument();
  assert.equal(validateValidationProfiles(document), document);
  const profile = document.profiles[0];
  const environment = document.environments[0];
  const revisions = new Map([[
    `${environment.id}@${environment.revision}`,
    environment,
  ]]);
  const ubuntuHost = {
    operatingSystem: "ubuntu",
    release: "24.04",
    architecture: "x86_64",
  };
  assert.equal(
    selectValidationEnvironmentFrom(profile, ubuntuHost, revisions, {
      environmentId: environment.id,
      runtimeOperatingSystem: "linux",
    }),
    environment,
  );
  assert.deepEqual(resolveValidationProfile(profile, environment).sandbox, {
    ...profile.sandbox,
    limiter: "podman",
    limiterVersion: "5.4.2",
    runtime: "podman",
    runtimeVersion: "5.4.2",
  });
  assert.throws(
    () => selectValidationEnvironmentFrom(profile, ubuntuHost, revisions),
    /does not match exactly one supported environment/u,
  );
  assert.throws(
    () => selectValidationEnvironmentFrom(profile, ubuntuHost, revisions, {
      environmentId: environment.id,
      runtimeOperatingSystem: "darwin",
    }),
    /does not match exactly one supported environment/u,
  );

  const taggedImage = ociValidationDocument();
  taggedImage.environments[0].execution.image =
    "ghcr.io/example/validator:latest";
  assert.throws(
    () => validateValidationProfiles(taggedImage),
    /image is invalid/u,
  );
  const optionLikeImage = ociValidationDocument();
  optionLikeImage.environments[0].execution.image =
    `--rootfs@${ociDigest}`;
  assert.throws(
    () => validateValidationProfiles(optionLikeImage),
    /image is invalid/u,
  );
  const unsafeSource = ociValidationDocument();
  unsafeSource.environments[0].execution.source =
    "https://github.com/example/../validator";
  assert.throws(
    () => validateValidationProfiles(unsafeSource),
    /source is invalid/u,
  );
  const mismatchedRuntime = ociValidationDocument();
  mismatchedRuntime.environments[0].sandbox.runtime = {
    name: "bubblewrap",
    executable: "bwrap",
    version: "0.9.0",
    versionArgs: ["--version"],
  };
  assert.throws(
    () => validateValidationProfiles(mismatchedRuntime),
    /runtime is invalid/u,
  );
  const mismatchedLimiter = ociValidationDocument();
  mismatchedLimiter.environments[0].sandbox.limiter.versionArgs = [
    "version",
  ];
  assert.throws(
    () => validateValidationProfiles(mismatchedLimiter),
    /OCI sandbox tools differ/u,
  );
  const unsafeContainerRuntime = ociValidationDocument();
  unsafeContainerRuntime.environments[0].sandbox.containerRuntime.executable =
    "/usr/local/bin/crun";
  assert.throws(
    () => validateValidationProfiles(unsafeContainerRuntime),
    /container runtime is invalid/u,
  );
  const unsafeSeccompProfile = ociValidationDocument();
  unsafeSeccompProfile.environments[0].sandbox.seccompProfile.path =
    "/tmp/seccomp.json";
  assert.throws(
    () => validateValidationProfiles(unsafeSeccompProfile),
    /seccomp profile is invalid/u,
  );
  const missingConfigurationHash = ociValidationDocument();
  delete missingConfigurationHash.environments[0].sandbox.configurationSha256;
  assert.throws(
    () => validateValidationProfiles(missingConfigurationHash),
    /sandbox has unexpected fields/u,
  );

  const dependencyImage = ociValidationDocument();
  dependencyImage.profiles[0].dependencies = [{
    name: "example-package",
    source: "npm",
    version: "1.2.3",
  }];
  dependencyImage.profiles[0].dependencyInstall = {
    kind: "oci-image",
    image: imageReference,
  };
  assert.equal(validateValidationProfiles(dependencyImage), dependencyImage);
  assert.equal(
    sandboxProfileBlockReason(
      dependencyImage.profiles[0],
      dependencyImage.environments[0],
    ),
    null,
  );
  dependencyImage.profiles[0].dependencyInstall.image =
    `ghcr.io/example/other@${ociDigest}`;
  assert.throws(
    () => validateValidationProfiles(dependencyImage),
    /does not match its environments/u,
  );
});

test("the committed C11 OCI activation matches publication evidence", () => {
  const { definition, publication, runtimeContract } =
    loadOciImageActivation();
  const environment = getValidationEnvironmentRevision(
    "debian-12-x86-64-c11-oci",
    1,
  );
  const profile = getValidationProfile("c11-host");
  assert.equal(profile.revision, 4);
  assert.deepEqual(profile.environments, [
    { id: environment.id, revision: environment.revision },
    { id: "debian-13-x86-64-c11-host", revision: 1 },
    { id: "ubuntu-24-04-x86-64-c11-host", revision: 1 },
  ]);
  assert.deepEqual(environment.execution, {
    kind: "oci",
    image: publication.image,
    source: publication.source,
    revision: publication.sourceRevision,
  });
  assert.deepEqual(environment.sandbox, runtimeContract.sandbox);
  assert.deepEqual(environment.toolchains, definition.toolchains);
  assert.deepEqual(environment.host, {
    operatingSystem: definition.platform.distribution,
    release: definition.platform.release,
    architecture: definition.platform.validationArchitecture,
  });
  assert.equal(
    selectValidationEnvironment(
      profile,
      {
        operatingSystem: "ubuntu",
        release: "24.04",
        architecture: "x86_64",
      },
      { environmentId: environment.id, runtimeOperatingSystem: "linux" },
    ),
    environment,
  );
});

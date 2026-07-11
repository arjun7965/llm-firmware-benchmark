import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { createHash } from "node:crypto";
import test from "node:test";
import {
  environmentFingerprint,
  getValidationProfile,
  getValidationEnvironmentRevision,
  getValidationProfileRevision,
  normalizeDependencyLockfileContent,
  profileFingerprint,
  resolveValidationProfile,
  selectValidationEnvironment,
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
  const stableEnvironment = getValidationEnvironmentRevision(
    "ubuntu-24-04-x86-64-stable-rust",
    1,
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
    getValidationProfileRevision("stable-rust", 2),
    getValidationProfile("stable-rust"),
  );
  assert.equal(getValidationProfile("go-std").revision, 3);
  assert.equal(
    getValidationProfile("go-std").sandbox.resourceLimits.test
      .addressSpaceBytes,
    1024 * 1024 * 1024,
  );
  assert.equal(
    getValidationProfile("python3-stdlib").revision,
    3,
  );
  assert.deepEqual(
    getValidationProfile("python3-stdlib").testRuntime.mounts.map((mount) =>
      mount.path),
    ["/usr/bin/python3", "/usr/lib/python3.12"],
  );
  assert.equal(
    getValidationProfile("postgresql").testRuntime.commandContracts[0].id,
    "postgresql-script",
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
      profile.revision === 3)
    .dependencyInstall.sha256 = "0".repeat(64);
  assert.throws(
    () => validateValidationProfileLockfiles(
      changedLockfileHash,
      new URL("../", import.meta.url),
    ),
    /dependencyInstall sha256 does not match/u,
  );
  const lockfileContent = readFileSync(
    new URL("../validation-locks/node-typescript-v3.lock.json", import.meta.url),
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
      schemaVersion: "2.2",
      environments: structuredClone(validationEnvironments),
      profiles: [validProfile, secondRevision],
    }),
    /current validation profile c11-host@2 must use the logical/u,
  );
  assert.throws(
    () => validateValidationProfiles({
      schemaVersion: "2.2",
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
      schemaVersion: "2.2",
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
      schemaVersion: "2.2",
      environments: structuredClone(validationEnvironments),
      profiles: [validProfile, structuredClone(validProfile)],
    }),
    /sorted and contiguous/u,
  );
  assert.throws(
    () => validateValidationProfiles({
      schemaVersion: "2.2",
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
      schemaVersion: "2.2",
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
      operatingSystem: "debian",
      release: "13",
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
      profile.revision === 3)
    .dependencyInstall;
  assert.throws(
    () => validateValidationProfiles(missingInstall),
    /current validation profile node-typescript@3 must define dependencyInstall/u,
  );
  const mismatchedInstall = structuredClone(validationProfilesDocument);
  mismatchedInstall.profiles
    .find((profile) => profile.id === "node-typescript" &&
      profile.revision === 3)
    .dependencyInstall.source = "pypi";
  assert.throws(
    () => validateValidationProfiles(mismatchedInstall),
    /dependencyInstall source does not cover/u,
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
});

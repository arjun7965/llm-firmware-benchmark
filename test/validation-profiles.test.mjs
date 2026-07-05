import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";
import {
  environmentFingerprint,
  getValidationProfile,
  getValidationEnvironmentRevision,
  getValidationProfileRevision,
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
      schemaVersion: "2.0",
      environments: structuredClone(validationEnvironments),
      profiles: [validProfile, secondRevision],
    }),
    /current validation profile c11-host@2 must use the logical/u,
  );
  assert.throws(
    () => validateValidationProfiles({
      schemaVersion: "2.0",
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
      schemaVersion: "2.0",
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
      schemaVersion: "2.0",
      environments: structuredClone(validationEnvironments),
      profiles: [validProfile, structuredClone(validProfile)],
    }),
    /sorted and contiguous/u,
  );
  assert.throws(
    () => validateValidationProfiles({
      schemaVersion: "2.0",
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
      schemaVersion: "2.0",
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
});

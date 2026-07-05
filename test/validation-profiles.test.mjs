import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";
import {
  getValidationProfile,
  getValidationProfileRevision,
  profileFingerprint,
  validateValidationProfileFingerprints,
  validateValidationProfiles,
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
  for (const profile of validationProfiles) {
    assert.ok(Number.isSafeInteger(profile.revision));
    assert.ok(profile.revision >= 1);
    assert.equal(profile.host.release, "24.04");
    assert.ok(profile.toolchains.length > 0);
    assert.ok(
      profile.toolchains.every((toolchain) =>
        /^\d+\.\d+(?:\.\d+)?/u.test(toolchain.version)),
    );
    assert.match(profileFingerprint(profile), /^[a-f0-9]{64}$/u);
    assert.equal(Object.isFrozen(profile), true);
    assert.equal(Object.isFrozen(profile.sandbox.resourceLimits.test), true);
  }
  assert.equal(
    getValidationProfile("stable-rust").toolchains
      .find((toolchain) => toolchain.name === "rustc").version,
    "1.87.0",
  );
  assert.equal(
    getValidationProfileRevision("stable-rust", 1),
    getValidationProfile("stable-rust"),
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
  const validProfile = structuredClone(validationProfiles[0]);
  const secondRevision = {
    ...structuredClone(validProfile),
    revision: 2,
  };
  assert.doesNotThrow(
    () => validateValidationProfiles({
      schemaVersion: "1.0",
      profiles: [validProfile, secondRevision],
    }),
  );
  assert.throws(
    () => validateValidationProfiles({
      schemaVersion: "1.0",
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
      schemaVersion: "1.0",
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
      schemaVersion: "1.0",
      profiles: [validProfile, structuredClone(validProfile)],
    }),
    /sorted and contiguous/u,
  );
  assert.throws(
    () => validateValidationProfiles({
      schemaVersion: "1.0",
      profiles: [{
        ...structuredClone(validProfile),
        revision: 2,
      }],
    }),
    /must start at 1/u,
  );
});

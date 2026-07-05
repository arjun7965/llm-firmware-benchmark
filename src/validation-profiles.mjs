import { createHash } from "node:crypto";
import { readFileSync } from "node:fs";

const profileIdPattern = /^[a-z][a-z0-9-]*$/u;
const packageNamePattern = /^(?:@[a-z0-9._-]+\/)?[a-z0-9][a-z0-9._-]*$/u;
const toolNamePattern = /^[a-z0-9][a-z0-9+._-]*$/u;
const versionPattern =
  /^\d+\.\d+(?:\.\d+)?(?:[-+][A-Za-z0-9.-]+)?$/u;
const fingerprintPattern = /^[a-f0-9]{64}$/u;
const resourceLimitFields = [
  "addressSpaceBytes",
  "cpuSeconds",
  "fileBytes",
  "openFiles",
];
const phaseNames = ["compile", "test"];

function requireObject(value, name) {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    throw new TypeError(`${name} must be an object`);
  }
}

function requireExactFields(value, fields, name) {
  requireObject(value, name);
  if (Object.keys(value).sort().join(",") !== [...fields].sort().join(",")) {
    throw new TypeError(`${name} has unexpected fields`);
  }
}

function requireString(value, name, pattern) {
  if (typeof value !== "string" || !pattern.test(value)) {
    throw new TypeError(`${name} is invalid`);
  }
}

function requirePositiveInteger(value, name) {
  if (!Number.isSafeInteger(value) || value < 1) {
    throw new TypeError(`${name} must be a positive safe integer`);
  }
}

function requireSortedUnique(values, name, key) {
  const identifiers = values.map(key);
  if (
    new Set(identifiers).size !== identifiers.length ||
    identifiers.join(",") !== [...identifiers].sort().join(",")
  ) {
    throw new TypeError(`${name} must be sorted and unique`);
  }
}

function validateProfileRevisionHistory(profiles) {
  let previous = null;
  for (const profile of profiles) {
    if (
      previous &&
      (
        profile.id < previous.id ||
        (
          profile.id === previous.id &&
          profile.revision !== previous.revision + 1
        )
      )
    ) {
      throw new TypeError(
        "validation profile revisions must be sorted and contiguous",
      );
    }
    if (
      (!previous || profile.id !== previous.id) &&
      profile.revision !== 1
    ) {
      throw new TypeError(
        `validation profile ${profile.id} revisions must start at 1`,
      );
    }
    previous = profile;
  }
}

function validateToolchains(toolchains, profileId) {
  if (!Array.isArray(toolchains) || toolchains.length === 0) {
    throw new TypeError(
      `validation profile ${profileId} toolchains must be non-empty`,
    );
  }
  for (const toolchain of toolchains) {
    requireExactFields(
      toolchain,
      ["name", "version", "versionArgs"],
      `validation profile ${profileId} toolchain`,
    );
    requireString(
      toolchain.name,
      `validation profile ${profileId} toolchain name`,
      toolNamePattern,
    );
    requireString(
      toolchain.version,
      `validation profile ${profileId} toolchain version`,
      versionPattern,
    );
    if (
      !Array.isArray(toolchain.versionArgs) ||
      toolchain.versionArgs.length === 0 ||
      toolchain.versionArgs.some((argument) =>
        typeof argument !== "string" ||
        argument.length === 0 ||
        argument.includes("\0"))
    ) {
      throw new TypeError(
        `validation profile ${profileId} versionArgs is invalid`,
      );
    }
  }
  requireSortedUnique(
    toolchains,
    `validation profile ${profileId} toolchains`,
    (toolchain) => toolchain.name,
  );
}

function validateDependencies(dependencies, profileId) {
  if (!Array.isArray(dependencies)) {
    throw new TypeError(
      `validation profile ${profileId} dependencies must be an array`,
    );
  }
  for (const dependency of dependencies) {
    requireExactFields(
      dependency,
      ["name", "source", "version"],
      `validation profile ${profileId} dependency`,
    );
    requireString(
      dependency.name,
      `validation profile ${profileId} dependency name`,
      packageNamePattern,
    );
    if (!["npm", "pypi"].includes(dependency.source)) {
      throw new TypeError(
        `validation profile ${profileId} dependency source is invalid`,
      );
    }
    requireString(
      dependency.version,
      `validation profile ${profileId} dependency version`,
      versionPattern,
    );
  }
  requireSortedUnique(
    dependencies,
    `validation profile ${profileId} dependencies`,
    (dependency) => `${dependency.source}:${dependency.name}`,
  );
}

function validateSandbox(sandbox, profileId) {
  requireExactFields(
    sandbox,
    [
      "filesystem",
      "limiter",
      "limiterVersion",
      "network",
      "resourceLimits",
      "rootTmpfsBytes",
      "runtime",
      "runtimeVersion",
      "temporaryDirectoryBytes",
    ],
    `validation profile ${profileId} sandbox`,
  );
  if (
    sandbox.runtime !== "bubblewrap" ||
    sandbox.limiter !== "prlimit" ||
    sandbox.network !== "none" ||
    sandbox.filesystem !== "isolated"
  ) {
    throw new TypeError(
      `validation profile ${profileId} sandbox policy is invalid`,
    );
  }
  requireString(
    sandbox.runtimeVersion,
    `validation profile ${profileId} sandbox runtimeVersion`,
    versionPattern,
  );
  requireString(
    sandbox.limiterVersion,
    `validation profile ${profileId} sandbox limiterVersion`,
    versionPattern,
  );
  requirePositiveInteger(
    sandbox.rootTmpfsBytes,
    `validation profile ${profileId} sandbox rootTmpfsBytes`,
  );
  requirePositiveInteger(
    sandbox.temporaryDirectoryBytes,
    `validation profile ${profileId} sandbox temporaryDirectoryBytes`,
  );
  requireExactFields(
    sandbox.resourceLimits,
    phaseNames,
    `validation profile ${profileId} resourceLimits`,
  );
  for (const phase of phaseNames) {
    const limits = sandbox.resourceLimits[phase];
    requireExactFields(
      limits,
      resourceLimitFields,
      `validation profile ${profileId} ${phase} limits`,
    );
    for (const [name, value] of Object.entries(limits)) {
      requirePositiveInteger(
        value,
        `validation profile ${profileId} ${phase}.${name}`,
      );
    }
  }
}

export function validateValidationProfiles(document) {
  requireExactFields(
    document,
    ["profiles", "schemaVersion"],
    "validation profiles document",
  );
  if (document.schemaVersion !== "1.0") {
    throw new TypeError("unsupported validation profiles schemaVersion");
  }
  if (!Array.isArray(document.profiles) || document.profiles.length === 0) {
    throw new TypeError("validation profiles must be a non-empty array");
  }
  for (const profile of document.profiles) {
    requireExactFields(
      profile,
      [
        "dependencies",
        "host",
        "id",
        "revision",
        "sandbox",
        "toolchains",
      ],
      "validation profile",
    );
    requireString(profile.id, "validation profile id", profileIdPattern);
    requirePositiveInteger(
      profile.revision,
      `validation profile ${profile.id} revision`,
    );
    requireExactFields(
      profile.host,
      ["architecture", "operatingSystem", "release"],
      `validation profile ${profile.id} host`,
    );
    if (
      profile.host.operatingSystem !== "ubuntu" ||
      profile.host.release !== "24.04" ||
      profile.host.architecture !== "x86_64"
    ) {
      throw new TypeError(
        `validation profile ${profile.id} host is invalid`,
      );
    }
    validateToolchains(profile.toolchains, profile.id);
    validateDependencies(profile.dependencies, profile.id);
    validateSandbox(profile.sandbox, profile.id);
  }
  validateProfileRevisionHistory(document.profiles);
  return document;
}

function deepFreeze(value) {
  if (value && typeof value === "object" && !Object.isFrozen(value)) {
    for (const child of Object.values(value)) deepFreeze(child);
    Object.freeze(value);
  }
  return value;
}

function canonicalize(value) {
  if (Array.isArray(value)) return value.map(canonicalize);
  if (value && typeof value === "object") {
    return Object.fromEntries(
      Object.keys(value)
        .sort()
        .map((key) => [key, canonicalize(value[key])]),
    );
  }
  return value;
}

export function profileFingerprint(profile) {
  return createHash("sha256")
    .update(JSON.stringify(canonicalize(profile)))
    .digest("hex");
}

export function validateValidationProfileFingerprints(
  profilesDocument,
  fingerprintsDocument,
) {
  validateValidationProfiles(profilesDocument);
  requireExactFields(
    fingerprintsDocument,
    ["profiles", "schemaVersion"],
    "validation profile fingerprints document",
  );
  if (fingerprintsDocument.schemaVersion !== "1.0") {
    throw new TypeError(
      "unsupported validation profile fingerprints schemaVersion",
    );
  }
  requireObject(
    fingerprintsDocument.profiles,
    "validation profile fingerprints",
  );
  const profileIds = [
    ...new Set(profilesDocument.profiles.map((profile) => profile.id)),
  ];
  requireExactFields(
    fingerprintsDocument.profiles,
    profileIds,
    "validation profile fingerprints",
  );
  for (const profileId of profileIds) {
    const profiles = profilesDocument.profiles
      .filter((profile) => profile.id === profileId);
    const revisions = profiles.map((profile) => String(profile.revision));
    const fingerprints = fingerprintsDocument.profiles[profileId];
    requireExactFields(
      fingerprints,
      revisions,
      `validation profile ${profileId} fingerprints`,
    );
    for (const profile of profiles) {
      const fingerprint = fingerprints[String(profile.revision)];
      if (
        typeof fingerprint !== "string" ||
        !fingerprintPattern.test(fingerprint) ||
        fingerprint !== profileFingerprint(profile)
      ) {
        throw new TypeError(
          `validation profile ${profileId}@${profile.revision} ` +
          "fingerprint is invalid",
        );
      }
    }
  }
  return fingerprintsDocument;
}

export function loadValidationProfiles(
  path = new URL("../validation-profiles.json", import.meta.url),
  fingerprintsPath = new URL(
    "../validation-profile-fingerprints.json",
    import.meta.url,
  ),
) {
  const document = JSON.parse(readFileSync(path, "utf8"));
  const fingerprints = JSON.parse(readFileSync(fingerprintsPath, "utf8"));
  validateValidationProfileFingerprints(document, fingerprints);
  return deepFreeze(document);
}

export const validationProfilesDocument = loadValidationProfiles();
export const validationProfiles = validationProfilesDocument.profiles;
export const validationProfileIds = Object.freeze(
  [...new Set(validationProfiles.map((profile) => profile.id))],
);
export const validationProfileSet = new Set(validationProfileIds);
export const sandboxRunnableValidationProfileIds = Object.freeze([
  "c11-host",
  "go-std",
  "stable-rust",
]);
const sandboxRunnableValidationProfileSet = new Set(
  sandboxRunnableValidationProfileIds,
);

const validationProfileMap = new Map();
const validationProfileRevisionMap = new Map();
for (const profile of validationProfiles) {
  validationProfileMap.set(profile.id, profile);
  validationProfileRevisionMap.set(
    `${profile.id}@${profile.revision}`,
    profile,
  );
}

export function requireValidationProfile(
  value,
  name = "validationProfile",
) {
  if (typeof value !== "string" || !validationProfileSet.has(value)) {
    throw new TypeError(`${name} is invalid`);
  }
  return value;
}

export function getValidationProfile(value, name = "validationProfile") {
  return validationProfileMap.get(requireValidationProfile(value, name));
}

export function getValidationProfileRevision(
  value,
  revision,
  name = "validationProfile",
) {
  requireValidationProfile(value, name);
  requirePositiveInteger(revision, `${name}Revision`);
  const profile = validationProfileRevisionMap.get(`${value}@${revision}`);
  if (!profile) {
    throw new TypeError(`${name}Revision is unknown`);
  }
  return profile;
}

export function sandboxProfileBlockReason(profile) {
  if (profile.dependencies.length > 0) {
    return "its validation profile dependencies are unverifiable";
  }
  if (!sandboxRunnableValidationProfileSet.has(profile.id)) {
    return "its validation profile requires an unavailable test runtime";
  }
  return null;
}

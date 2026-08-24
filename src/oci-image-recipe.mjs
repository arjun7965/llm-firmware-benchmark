import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { ociRuntimeConfigurationFingerprint } from "./oci-sandbox.mjs";

const digestPattern = /^sha256:[a-f0-9]{64}$/u;
const imageRepositoryPattern =
  /^(?:[a-z0-9](?:[a-z0-9.-]*[a-z0-9])?\/)?[a-z0-9](?:[a-z0-9._-]*[a-z0-9])?(?:\/[a-z0-9](?:[a-z0-9._-]*[a-z0-9])?)+$/u;
const sourcePattern =
  /^https:\/\/github\.com\/[A-Za-z0-9](?:[A-Za-z0-9_.-]*[A-Za-z0-9])?\/[A-Za-z0-9](?:[A-Za-z0-9_.-]*[A-Za-z0-9])?$/u;
const identifierPattern = /^[a-z][a-z0-9-]*$/u;
const toolNamePattern = /^[a-z0-9][a-z0-9+._-]*$/u;
const immutableBasePattern =
  /^docker\.io\/[a-z0-9](?:[a-z0-9._-]*[a-z0-9])?(?:\/[a-z0-9](?:[a-z0-9._-]*[a-z0-9])?)+@sha256:[a-f0-9]{64}$/u;
const documentedIndexPattern =
  /^docker\.io\/[a-z0-9](?:[a-z0-9._-]*[a-z0-9])?(?:\/[a-z0-9](?:[a-z0-9._-]*[a-z0-9])?)+:[A-Za-z0-9_][A-Za-z0-9._-]{0,127}@sha256:[a-f0-9]{64}$/u;
const sourceRevisionPattern = /^[a-f0-9]{40}$/u;
const forbiddenRecipePattern =
  /(?:^|\s)(?:ADD|COPY)\s|\b(?:apt|apt-get|apk|dnf|microdnf|yum|pip|npm|curl|wget)\b|https?:\/\//iu;

export const defaultOciImageRecipePath = fileURLToPath(
  new URL("../oci/c11/image-recipe.json", import.meta.url),
);
export const defaultOciContainerfilePath = fileURLToPath(
  new URL("../oci/c11/Containerfile", import.meta.url),
);
export const defaultOciPublicationPath = fileURLToPath(
  new URL("../oci/c11/publication.json", import.meta.url),
);
export const defaultOciRuntimeContractPath = fileURLToPath(
  new URL("../oci/c11/runtime-contract.json", import.meta.url),
);

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

function digestFromReference(reference) {
  return reference.slice(reference.lastIndexOf("@") + 1);
}

function logicalInstructions(containerfile) {
  if (
    typeof containerfile !== "string" ||
    containerfile.length === 0 ||
    containerfile.includes("\0") ||
    containerfile.includes("\r")
  ) {
    throw new TypeError("OCI Containerfile text is invalid");
  }
  const instructions = [];
  let pending = "";
  for (const rawLine of containerfile.split("\n")) {
    const line = rawLine.trim();
    if (line === "" || line.startsWith("#")) continue;
    pending += pending === "" ? line : ` ${line}`;
    if (pending.endsWith("\\")) {
      pending = pending.slice(0, -1).trimEnd();
      continue;
    }
    instructions.push(pending);
    pending = "";
  }
  if (pending !== "") {
    throw new TypeError("OCI Containerfile has an unfinished continuation");
  }
  return instructions;
}

export function validateOciImageRecipeDefinition(definition) {
  requireExactFields(
    definition,
    [
      "base",
      "id",
      "imageRepository",
      "platform",
      "schemaVersion",
      "source",
      "toolchains",
      "user",
    ],
    "OCI image recipe",
  );
  if (definition.schemaVersion !== "1.0") {
    throw new TypeError("OCI image recipe schemaVersion is unsupported");
  }
  requireString(definition.id, "OCI image recipe id", identifierPattern);
  requireString(
    definition.imageRepository,
    "OCI image repository",
    imageRepositoryPattern,
  );
  requireString(definition.source, "OCI image source", sourcePattern);
  requireExactFields(
    definition.base,
    ["indexReference", "reference"],
    "OCI image base",
  );
  requireString(
    definition.base.reference,
    "OCI image platform base",
    immutableBasePattern,
  );
  requireString(
    definition.base.indexReference,
    "OCI image index base",
    documentedIndexPattern,
  );
  if (
    digestFromReference(definition.base.reference) ===
      digestFromReference(definition.base.indexReference)
  ) {
    throw new TypeError(
      "OCI image platform and index bases must use distinct manifest digests",
    );
  }
  requireExactFields(
    definition.platform,
    [
      "architecture",
      "distribution",
      "operatingSystem",
      "release",
      "validationArchitecture",
    ],
    "OCI image platform",
  );
  if (
    definition.platform.operatingSystem !== "linux" ||
    definition.platform.architecture !== "amd64" ||
    definition.platform.validationArchitecture !== "x86_64" ||
    definition.platform.distribution !== "debian" ||
    definition.platform.release !== "12"
  ) {
    throw new TypeError("OCI image platform is unsupported");
  }
  requireExactFields(
    definition.user,
    ["gid", "home", "name", "uid"],
    "OCI image user",
  );
  if (
    definition.user.name !== "validator" ||
    definition.user.home !== "/nonexistent"
  ) {
    throw new TypeError("OCI image user identity is invalid");
  }
  requirePositiveInteger(definition.user.uid, "OCI image uid");
  requirePositiveInteger(definition.user.gid, "OCI image gid");
  if (definition.user.uid !== 65_532 || definition.user.gid !== 65_532) {
    throw new TypeError("OCI image user must be UID/GID 65532");
  }
  if (!Array.isArray(definition.toolchains) || definition.toolchains.length < 1) {
    throw new TypeError("OCI image toolchains must be non-empty");
  }
  for (const toolchain of definition.toolchains) {
    requireExactFields(
      toolchain,
      ["name", "version", "versionArgs"],
      "OCI image toolchain",
    );
    requireString(toolchain.name, "OCI image toolchain name", toolNamePattern);
    requireString(
      toolchain.version,
      "OCI image toolchain version",
      /^\d+\.\d+(?:\.\d+)?$/u,
    );
    if (
      !Array.isArray(toolchain.versionArgs) ||
      toolchain.versionArgs.length === 0 ||
      toolchain.versionArgs.some((argument) =>
        typeof argument !== "string" ||
        argument.length === 0 ||
        argument.includes("\0"))
    ) {
      throw new TypeError("OCI image toolchain versionArgs are invalid");
    }
  }
  const toolchainNames = definition.toolchains.map(({ name }) => name);
  if (
    new Set(toolchainNames).size !== toolchainNames.length ||
    toolchainNames.join(",") !== [...toolchainNames].sort().join(",")
  ) {
    throw new TypeError("OCI image toolchains must be sorted and unique");
  }
  return definition;
}

export function validateOciContainerfile(containerfile, definition) {
  validateOciImageRecipeDefinition(definition);
  const instructions = logicalInstructions(containerfile);
  const from = instructions.filter((line) => /^FROM\s/iu.test(line));
  if (from.length !== 1 || from[0] !== `FROM ${definition.base.reference}`) {
    throw new TypeError("OCI Containerfile base does not match its recipe");
  }
  if (instructions.some((line) =>
    /^(?:ADD|COPY|RUN)\s/iu.test(line) && forbiddenRecipePattern.test(line))) {
    throw new TypeError(
      "OCI Containerfile may not fetch, install, add, or copy mutable content",
    );
  }
  if (instructions.filter((line) => line === "ARG SOURCE_REVISION").length !== 1) {
    throw new TypeError("OCI Containerfile must require SOURCE_REVISION");
  }
  const expectedLabels = [
    `LABEL org.opencontainers.image.source="${definition.source}"`,
    "LABEL org.opencontainers.image.revision=\"${SOURCE_REVISION}\"",
    "LABEL org.opencontainers.image.licenses=\"Apache-2.0\"",
  ];
  for (const label of expectedLabels) {
    if (instructions.filter((line) => line === label).length !== 1) {
      throw new TypeError("OCI Containerfile provenance labels are incomplete");
    }
  }
  const accountCommand =
    "RUN printf '%s\\n' 'validator:x:65532:' >> /etc/group && " +
    "printf '%s\\n' " +
    "'validator:x:65532:65532:OCI validator:/nonexistent:/usr/sbin/nologin' " +
    ">> /etc/passwd";
  if (instructions.filter((line) => line === accountCommand).length !== 1) {
    throw new TypeError("OCI Containerfile user definition is not deterministic");
  }
  if (
    instructions.filter((line) =>
      line === `USER ${definition.user.uid}:${definition.user.gid}`).length !== 1
  ) {
    throw new TypeError("OCI Containerfile must select UID/GID 65532");
  }
  if (instructions.filter((line) => line === "WORKDIR /workspace").length !== 1) {
    throw new TypeError("OCI Containerfile workdir is invalid");
  }
  return definition;
}

export function loadOciImageRecipe({
  recipePath = defaultOciImageRecipePath,
  containerfilePath = defaultOciContainerfilePath,
} = {}) {
  const definition = JSON.parse(readFileSync(recipePath, "utf8"));
  const containerfile = readFileSync(containerfilePath, "utf8");
  validateOciContainerfile(containerfile, definition);
  return { containerfile, definition };
}

export function validateOciPublication(publication, definition) {
  validateOciImageRecipeDefinition(definition);
  requireExactFields(
    publication,
    [
      "base",
      "image",
      "platform",
      "platformManifestDigest",
      "schemaVersion",
      "source",
      "sourceRevision",
    ],
    "OCI image publication",
  );
  if (publication.schemaVersion !== "1.0") {
    throw new TypeError("OCI image publication schemaVersion is unsupported");
  }
  requireString(
    publication.platformManifestDigest,
    "OCI image publication digest",
    digestPattern,
  );
  if (
    publication.image !==
      `${definition.imageRepository}@${publication.platformManifestDigest}` ||
    publication.base !== definition.base.reference ||
    publication.source !== definition.source ||
    publication.platform !==
      `${definition.platform.operatingSystem}/${definition.platform.architecture}`
  ) {
    throw new TypeError("OCI image publication does not match its recipe");
  }
  requireString(
    publication.sourceRevision,
    "OCI image publication source revision",
    sourceRevisionPattern,
  );
  return publication;
}

function validateRuntimeTool(tool, expected, name) {
  requireExactFields(
    tool,
    ["executable", "name", "version", "versionArgs"],
    name,
  );
  if (tool.name !== expected.name || tool.executable !== expected.executable) {
    throw new TypeError(`${name} identity is invalid`);
  }
  requireString(tool.version, `${name} version`, /^\d+\.\d+(?:\.\d+)?$/u);
  if (
    !Array.isArray(tool.versionArgs) ||
    tool.versionArgs.length !== 1 ||
    tool.versionArgs[0] !== "--version"
  ) {
    throw new TypeError(`${name} versionArgs are invalid`);
  }
}

export function validateOciRuntimeContract(contract) {
  requireExactFields(
    contract,
    ["runner", "sandbox", "schemaVersion"],
    "OCI runtime contract",
  );
  if (contract.schemaVersion !== "1.0") {
    throw new TypeError("OCI runtime contract schemaVersion is unsupported");
  }
  requireExactFields(
    contract.runner,
    ["architecture", "operatingSystem", "release"],
    "OCI runtime runner",
  );
  if (
    contract.runner.operatingSystem !== "ubuntu" ||
    contract.runner.release !== "24.04" ||
    contract.runner.architecture !== "x86_64"
  ) {
    throw new TypeError("OCI runtime runner is unsupported");
  }
  requireExactFields(
    contract.sandbox,
    [
      "configurationSha256",
      "containerRuntime",
      "limiter",
      "monitor",
      "runtime",
      "seccompProfile",
    ],
    "OCI runtime sandbox",
  );
  validateRuntimeTool(
    contract.sandbox.containerRuntime,
    { name: "crun", executable: "/usr/bin/crun" },
    "OCI container runtime",
  );
  validateRuntimeTool(
    contract.sandbox.monitor,
    { name: "conmon", executable: "/usr/bin/conmon" },
    "OCI monitor",
  );
  validateRuntimeTool(
    contract.sandbox.runtime,
    { name: "podman", executable: "podman" },
    "OCI sandbox runtime",
  );
  validateRuntimeTool(
    contract.sandbox.limiter,
    { name: "podman", executable: "podman" },
    "OCI sandbox limiter",
  );
  if (
    JSON.stringify(contract.sandbox.runtime) !==
      JSON.stringify(contract.sandbox.limiter)
  ) {
    throw new TypeError("OCI runtime and limiter contracts must match");
  }
  requireString(
    contract.sandbox.configurationSha256,
    "OCI runtime configuration fingerprint",
    /^[a-f0-9]{64}$/u,
  );
  if (
    contract.sandbox.configurationSha256 !==
      ociRuntimeConfigurationFingerprint({
        containerRuntime: contract.sandbox.containerRuntime,
        monitor: contract.sandbox.monitor,
      })
  ) {
    throw new TypeError("OCI runtime configuration fingerprint is invalid");
  }
  requireExactFields(
    contract.sandbox.seccompProfile,
    ["path", "sha256"],
    "OCI seccomp profile",
  );
  if (contract.sandbox.seccompProfile.path !==
    "/usr/share/containers/seccomp.json") {
    throw new TypeError("OCI seccomp profile path is invalid");
  }
  requireString(
    contract.sandbox.seccompProfile.sha256,
    "OCI seccomp profile fingerprint",
    /^[a-f0-9]{64}$/u,
  );
  return contract;
}

export function loadOciImageActivation({
  recipePath = defaultOciImageRecipePath,
  containerfilePath = defaultOciContainerfilePath,
  publicationPath = defaultOciPublicationPath,
  runtimeContractPath = defaultOciRuntimeContractPath,
} = {}) {
  const recipe = loadOciImageRecipe({ recipePath, containerfilePath });
  const publication = JSON.parse(readFileSync(publicationPath, "utf8"));
  const runtimeContract = JSON.parse(
    readFileSync(runtimeContractPath, "utf8"),
  );
  validateOciPublication(publication, recipe.definition);
  validateOciRuntimeContract(runtimeContract);
  return { ...recipe, publication, runtimeContract };
}

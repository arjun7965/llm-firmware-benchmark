import { spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import {
  existsSync,
  readFileSync,
} from "node:fs";

const identifierPattern = /^[a-z][a-z0-9-]*$/u;
const environmentVariablePattern = /^[A-Z][A-Z0-9_]*$/u;
const executablePattern = /^[A-Za-z0-9][A-Za-z0-9+._-]*$/u;
const expectedTestIds = Object.freeze([
  "probe-identity",
  "flash-verify",
  "reset-boot",
  "uart-handshake",
  "gpio-loopback",
  "watchdog-reset",
]);
const vendorContracts = new Map([
  [
    "st",
    Object.freeze({
      name: "STMicroelectronics",
      targetId: "stm32-nucleo-f446re",
      domains: Object.freeze(["st.com"]),
      probeVersionCommand: Object.freeze([
        "STM32_Programmer_CLI",
        "--version",
      ]),
      programmer: "STM32CubeProgrammer CLI",
      sdkEnvVar: "HIL_STM32CUBEF4_ROOT",
      sdkName: "STM32CubeF4",
    }),
  ],
  [
    "nxp",
    Object.freeze({
      name: "NXP Semiconductors",
      targetId: "nxp-frdm-mcxn947",
      domains: Object.freeze(["nxp.com"]),
      probeVersionCommand: Object.freeze(["LinkServer", "--version"]),
      programmer: "LinkServer",
      sdkEnvVar: "HIL_MCUXPRESSO_SDK_ROOT",
      sdkName: "MCUXpresso SDK for FRDM-MCXN947",
    }),
  ],
  [
    "ti",
    Object.freeze({
      name: "Texas Instruments",
      targetId: "ti-lp-mspm0g3507",
      domains: Object.freeze(["ti.com"]),
      probeVersionCommand: Object.freeze(["dslite", "--version"]),
      programmer: "UniFlash dslite",
      sdkEnvVar: "HIL_MSPM0_SDK_ROOT",
      sdkName: "MSPM0 SDK",
    }),
  ],
]);

export const hilTargetIds = Object.freeze(
  [...vendorContracts.values()].map((vendor) => vendor.targetId),
);

export const hilTestIds = expectedTestIds;

const catalogFields = ["policy", "schemaVersion", "targets"];
const policyFields = [
  "network",
  "publication",
  "requiredScoringPath",
  "role",
];
const targetFields = [
  "board",
  "id",
  "probe",
  "sdk",
  "testPlan",
  "toolchain",
  "vendor",
  "vendorName",
];
const boardFields = [
  "architecture",
  "datasheetUrl",
  "mcu",
  "name",
  "productUrl",
  "userManualUrl",
];
const probeFields = [
  "interface",
  "license",
  "name",
  "programmer",
  "toolUrl",
  "versionCommand",
  "versionPolicy",
];
const toolchainFields = [
  "downloadUrl",
  "license",
  "name",
  "targetTriple",
  "versionCommand",
  "versionPolicy",
];
const sdkFields = [
  "envVar",
  "license",
  "name",
  "required",
  "sourceUrl",
  "versionPolicy",
];
const licenseFields = ["name", "redistribution", "url"];
const testFields = ["description", "destructive", "id", "required"];

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
  if (
    typeof value !== "string" ||
    value.trim() === "" ||
    value.length > 2048 ||
    (pattern && !pattern.test(value))
  ) {
    throw new TypeError(`${name} is invalid`);
  }
}

function matchesDomain(hostname, domains) {
  return domains.some((domain) =>
    hostname === domain || hostname.endsWith(`.${domain}`));
}

function requireUrl(value, name, domains) {
  requireString(value, name);
  let parsed;
  try {
    parsed = new URL(value);
  } catch {
    throw new TypeError(`${name} must be a valid URL`);
  }
  if (
    parsed.protocol !== "https:" ||
    parsed.username ||
    parsed.password ||
    (domains && !matchesDomain(parsed.hostname, domains))
  ) {
    throw new TypeError(`${name} must use an approved HTTPS domain`);
  }
}

function requireCommand(value, name) {
  if (
    !Array.isArray(value) ||
    value.length < 2 ||
    value.length > 8 ||
    !executablePattern.test(value[0]) ||
    value.some((arg) =>
      typeof arg !== "string" ||
      arg.length === 0 ||
      arg.length > 256 ||
      arg.includes("\0"))
  ) {
    throw new TypeError(`${name} must be a fixed executable and argv array`);
  }
}

function validateLicense(license, name, domains) {
  requireExactFields(license, licenseFields, name);
  requireString(license.name, `${name}.name`);
  if (license.redistribution !== "not-vendored") {
    throw new TypeError(`${name}.redistribution must be not-vendored`);
  }
  requireUrl(license.url, `${name}.url`, domains);
}

function validateTarget(target, index) {
  const name = `HIL target ${index}`;
  requireExactFields(target, targetFields, name);
  requireString(target.id, `${name}.id`, identifierPattern);
  requireString(target.vendor, `${name}.vendor`, identifierPattern);
  const vendor = vendorContracts.get(target.vendor);
  if (!vendor || target.id !== vendor.targetId) {
    throw new TypeError(`${name} has an unsupported vendor or target ID`);
  }
  if (target.vendorName !== vendor.name) {
    throw new TypeError(`${name}.vendorName does not match its vendor`);
  }

  requireExactFields(target.board, boardFields, `${name}.board`);
  for (const field of ["architecture", "mcu", "name"]) {
    requireString(target.board[field], `${name}.board.${field}`);
  }
  for (const field of ["datasheetUrl", "productUrl", "userManualUrl"]) {
    requireUrl(
      target.board[field],
      `${name}.board.${field}`,
      vendor.domains,
    );
  }

  requireExactFields(target.probe, probeFields, `${name}.probe`);
  for (const field of [
    "interface",
    "name",
    "programmer",
    "versionPolicy",
  ]) {
    requireString(target.probe[field], `${name}.probe.${field}`);
  }
  if (target.probe.interface !== "SWD") {
    throw new TypeError(`${name}.probe.interface must be SWD`);
  }
  if (target.probe.programmer !== vendor.programmer) {
    throw new TypeError(`${name}.probe.programmer is not approved`);
  }
  requireUrl(target.probe.toolUrl, `${name}.probe.toolUrl`, vendor.domains);
  requireCommand(target.probe.versionCommand, `${name}.probe.versionCommand`);
  if (
    target.probe.versionCommand.join("\0") !==
    vendor.probeVersionCommand.join("\0")
  ) {
    throw new TypeError(`${name}.probe.versionCommand is not approved`);
  }
  validateLicense(
    target.probe.license,
    `${name}.probe.license`,
    vendor.domains,
  );

  requireExactFields(target.toolchain, toolchainFields, `${name}.toolchain`);
  for (const field of ["name", "targetTriple", "versionPolicy"]) {
    requireString(target.toolchain[field], `${name}.toolchain.${field}`);
  }
  if (target.toolchain.targetTriple !== "arm-none-eabi") {
    throw new TypeError(`${name}.toolchain.targetTriple is unsupported`);
  }
  if (target.toolchain.name !== "Arm GNU Toolchain") {
    throw new TypeError(`${name}.toolchain.name is unsupported`);
  }
  requireUrl(
    target.toolchain.downloadUrl,
    `${name}.toolchain.downloadUrl`,
    ["arm.com"],
  );
  requireCommand(
    target.toolchain.versionCommand,
    `${name}.toolchain.versionCommand`,
  );
  if (
    target.toolchain.versionCommand.join("\0") !==
    "arm-none-eabi-gcc\0--version"
  ) {
    throw new TypeError(`${name}.toolchain.versionCommand is not approved`);
  }
  validateLicense(
    target.toolchain.license,
    `${name}.toolchain.license`,
    ["arm.com"],
  );

  requireExactFields(target.sdk, sdkFields, `${name}.sdk`);
  for (const field of ["name", "versionPolicy"]) {
    requireString(target.sdk[field], `${name}.sdk.${field}`);
  }
  requireString(
    target.sdk.envVar,
    `${name}.sdk.envVar`,
    environmentVariablePattern,
  );
  if (target.sdk.envVar !== vendor.sdkEnvVar) {
    throw new TypeError(`${name}.sdk.envVar is not approved`);
  }
  if (target.sdk.name !== vendor.sdkName) {
    throw new TypeError(`${name}.sdk.name is not approved`);
  }
  if (target.sdk.required !== true) {
    throw new TypeError(`${name}.sdk.required must be true`);
  }
  requireUrl(target.sdk.sourceUrl, `${name}.sdk.sourceUrl`, vendor.domains);
  validateLicense(
    target.sdk.license,
    `${name}.sdk.license`,
    vendor.domains,
  );

  if (!Array.isArray(target.testPlan)) {
    throw new TypeError(`${name}.testPlan must be an array`);
  }
  const testIds = [];
  for (const [testIndex, testCase] of target.testPlan.entries()) {
    const testName = `${name}.testPlan[${testIndex}]`;
    requireExactFields(testCase, testFields, testName);
    requireString(testCase.id, `${testName}.id`, identifierPattern);
    requireString(testCase.description, `${testName}.description`);
    if (
      testCase.required !== true ||
      typeof testCase.destructive !== "boolean"
    ) {
      throw new TypeError(`${testName} has invalid safety metadata`);
    }
    testIds.push(testCase.id);
  }
  if (testIds.join(",") !== expectedTestIds.join(",")) {
    throw new TypeError(`${name}.testPlan must define the common HIL protocol`);
  }
  if (target.testPlan[0].destructive) {
    throw new TypeError(`${name} probe identity must be non-destructive`);
  }
}

export function validateHilCatalog(catalog) {
  requireExactFields(catalog, catalogFields, "HIL catalog");
  if (catalog.schemaVersion !== "1.0") {
    throw new TypeError("unsupported HIL catalog schemaVersion");
  }
  requireExactFields(catalog.policy, policyFields, "HIL policy");
  if (
    catalog.policy.role !== "supplemental-only" ||
    catalog.policy.requiredScoringPath !== "host-mocks" ||
    catalog.policy.network !== "disabled-during-execution" ||
    catalog.policy.publication !== "sanitized-reports-only"
  ) {
    throw new TypeError("HIL policy cannot affect required benchmark scoring");
  }
  if (!Array.isArray(catalog.targets)) {
    throw new TypeError("HIL targets must be an array");
  }
  catalog.targets.forEach(validateTarget);
  const targetIds = catalog.targets.map((target) => target.id);
  if (targetIds.join(",") !== hilTargetIds.join(",")) {
    throw new TypeError("HIL targets must cover STM32, NXP, and TI in order");
  }
  return catalog;
}

export function loadHilCatalog(
  file = new URL("../hil-targets.json", import.meta.url),
) {
  return validateHilCatalog(JSON.parse(readFileSync(file, "utf8")));
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

export function hilCatalogReference(catalog) {
  validateHilCatalog(catalog);
  return {
    schemaVersion: catalog.schemaVersion,
    sha256: createHash("sha256")
      .update(JSON.stringify(canonicalize(catalog)))
      .digest("hex"),
  };
}

export function selectHilTargets(catalog, targetIds = []) {
  validateHilCatalog(catalog);
  if (!Array.isArray(targetIds)) {
    throw new TypeError("HIL target selection must be an array");
  }
  const selectedIds = targetIds.length === 0 ? hilTargetIds : targetIds;
  if (new Set(selectedIds).size !== selectedIds.length) {
    throw new TypeError("HIL target selection contains duplicates");
  }
  for (const id of selectedIds) {
    if (!hilTargetIds.includes(id)) {
      throw new TypeError(`Unknown HIL target: ${id}`);
    }
  }
  const selectedSet = new Set(selectedIds);
  return catalog.targets.filter((target) => selectedSet.has(target.id));
}

function firstOutputLine(result) {
  return `${result.stdout ?? ""}\n${result.stderr ?? ""}`
    .split(/\r?\n/u)
    .map((line) => line.trim())
    .find(Boolean)
    ?.slice(0, 1024) ?? "version output unavailable";
}

function checkCommand(command, options) {
  const result = options.spawn(command[0], command.slice(1), {
    cwd: options.cwd,
    encoding: "utf8",
    maxBuffer: 1024 * 1024,
    shell: false,
    timeout: 10000,
    windowsHide: true,
  });
  if (result.error || result.status !== 0) {
    return {
      available: false,
      detail: result.error?.code ?? `exit ${result.status}`,
    };
  }
  return {
    available: true,
    detail: firstOutputLine(result),
  };
}

export function checkHilReadiness(catalog, options = {}) {
  const settings = {
    cwd: process.cwd(),
    env: process.env,
    log: console.log,
    pathExists: existsSync,
    requireTools: false,
    spawn: spawnSync,
    targetIds: [],
    ...options,
  };
  const targets = selectHilTargets(catalog, settings.targetIds);
  const dependencies = [];
  const readyTargets = [];
  const skippedTargets = [];

  for (const target of targets) {
    const checks = [
      {
        kind: "toolchain",
        name: target.toolchain.name,
        result: checkCommand(target.toolchain.versionCommand, settings),
      },
      {
        kind: "probe-tool",
        name: target.probe.programmer,
        result: checkCommand(target.probe.versionCommand, settings),
      },
    ];
    const sdkPath = settings.env[target.sdk.envVar];
    const sdkConfigured = typeof sdkPath === "string" && sdkPath !== "";
    const sdkAvailable = sdkConfigured && settings.pathExists(sdkPath);
    checks.push({
      kind: "sdk",
      name: target.sdk.name,
      result: {
        available: sdkAvailable,
        detail: sdkAvailable
          ? `${target.sdk.envVar} is configured`
          : `${target.sdk.envVar} is ${sdkConfigured ? "invalid" : "unset"}`,
      },
    });

    const missing = [];
    for (const check of checks) {
      const dependency = {
        available: check.result.available,
        detail: check.result.detail,
        kind: check.kind,
        name: check.name,
        targetId: target.id,
      };
      dependencies.push(dependency);
      if (!dependency.available) missing.push(dependency);
    }
    if (missing.length === 0) {
      readyTargets.push(target.id);
      settings.log(`HIL tools ready: ${target.id}`);
    } else {
      skippedTargets.push(target.id);
      settings.log(
        `HIL tools unavailable for ${target.id}: ` +
        missing.map((item) => item.name).join(", "),
      );
    }
  }

  if (settings.requireTools && skippedTargets.length > 0) {
    throw new Error(
      `HIL dependencies are required for: ${skippedTargets.join(", ")}`,
    );
  }
  return {
    checkedTargetCount: targets.length,
    dependencies,
    readyTargets,
    skippedTargets,
  };
}

function readOptionValue(args, index, option) {
  const value = args[index + 1];
  if (!value || value.startsWith("--")) {
    throw new TypeError(`${option} requires a value`);
  }
  return value;
}

export function parseHilArgs(args) {
  const options = {
    help: false,
    probeTools: false,
    requireTools: false,
    targetIds: [],
  };
  for (let index = 0; index < args.length; index += 1) {
    const arg = args[index];
    if (arg === "--help") {
      options.help = true;
    } else if (arg === "--probe-tools") {
      options.probeTools = true;
    } else if (arg === "--require-tools") {
      options.probeTools = true;
      options.requireTools = true;
    } else if (arg === "--target") {
      options.targetIds.push(...readOptionValue(args, index, arg).split(","));
      index += 1;
    } else if (arg.startsWith("--target=")) {
      options.targetIds.push(...arg.slice("--target=".length).split(","));
    } else {
      throw new TypeError(`Unknown HIL option: ${arg}`);
    }
  }
  for (const id of options.targetIds) {
    if (!hilTargetIds.includes(id)) {
      throw new TypeError(`Unknown HIL target: ${id}`);
    }
  }
  if (new Set(options.targetIds).size !== options.targetIds.length) {
    throw new TypeError("HIL target selection contains duplicates");
  }
  return options;
}

export const hilUsage = `Usage: npm run hil:check -- [options]

Validate the optional hardware-in-the-loop catalog. No hardware is modified.

Options:
  --target <id[,id...]>  Select one or more exact HIL target IDs
  --probe-tools          Check compiler, programmer, and SDK availability
  --require-tools        Fail when any selected dependency is unavailable
  --help                 Show this help`;

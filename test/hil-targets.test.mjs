import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { test } from "node:test";

import {
  checkHilReadiness,
  hilCatalogReference,
  hilTargetIds,
  hilTestIds,
  loadHilCatalog,
  parseHilArgs,
  selectHilTargets,
  validateHilCatalog,
} from "../src/hil-targets.mjs";
import { validateHilReport } from "../src/hil-reports.mjs";

function validHilReport(catalog) {
  const target = catalog.targets[0];
  return {
    schemaVersion: "1.0",
    catalog: hilCatalogReference(catalog),
    targetId: target.id,
    startedAt: "2026-08-19T20:00:00.000Z",
    finishedAt: "2026-08-19T20:01:00.000Z",
    board: {
      hardwareRevision: "C-04",
      probeSerialSha256: "a".repeat(64),
      wiringRevision: "lab-loopback-v1",
    },
    firmware: {
      sourceRevision: "b".repeat(40),
      imageSha256: "c".repeat(64),
    },
    dependencies: [
      {
        kind: "probe-tool",
        name: target.probe.programmer,
        version: "2.20.0",
      },
      {
        kind: "toolchain",
        name: target.toolchain.name,
        version: "arm-none-eabi-gcc 15.2.1",
      },
      {
        kind: "sdk",
        name: target.sdk.name,
        version: "1.28.0",
      },
    ],
    tests: hilTestIds.map((id, index) => ({
      id,
      status: "pass",
      durationMs: index + 1,
      evidenceSha256: `${index}`.repeat(64),
    })),
    success: true,
  };
}

test("HIL catalog defines representative STM32, NXP, and TI boards", () => {
  const catalog = loadHilCatalog();

  assert.equal(catalog.policy.role, "supplemental-only");
  assert.equal(catalog.policy.requiredScoringPath, "host-mocks");
  assert.deepEqual(
    catalog.targets.map((target) => target.id),
    hilTargetIds,
  );
  assert.deepEqual(
    catalog.targets.map((target) => target.vendor),
    ["st", "nxp", "ti"],
  );
  for (const target of catalog.targets) {
    assert.deepEqual(
      target.testPlan.map((testCase) => testCase.id),
      hilTestIds,
    );
    assert.equal(target.testPlan[0].destructive, false);
    assert.ok(target.testPlan.slice(1).every((testCase) =>
      testCase.destructive));
    for (const url of [
      target.board.productUrl,
      target.board.datasheetUrl,
      target.board.userManualUrl,
    ]) {
      assert.equal(new URL(url).protocol, "https:");
    }
    for (const dependency of [target.probe, target.toolchain, target.sdk]) {
      assert.equal(dependency.license.redistribution, "not-vendored");
      assert.equal(new URL(dependency.license.url).protocol, "https:");
    }
  }
});

test("HIL guide links every board document and dependency authority", () => {
  const catalog = loadHilCatalog();
  const guide = readFileSync(
    new URL(
      "../docs/embedded/hardware-in-the-loop.md",
      import.meta.url,
    ),
    "utf8",
  );

  for (const target of catalog.targets) {
    assert.ok(guide.includes(`\`${target.id}\``));
    for (const url of [
      target.board.productUrl,
      target.board.datasheetUrl,
      target.board.userManualUrl,
      target.probe.toolUrl,
      target.probe.license.url,
      target.sdk.sourceUrl,
      target.sdk.license.url,
      target.toolchain.downloadUrl,
    ]) {
      assert.ok(guide.includes(url), `HIL guide is missing ${url}`);
    }
  }
});

test("HIL catalog rejects scoring changes and unofficial board links", () => {
  const catalog = structuredClone(loadHilCatalog());
  catalog.policy.role = "scored";
  assert.throws(
    () => validateHilCatalog(catalog),
    /cannot affect required benchmark scoring/u,
  );

  const unofficial = structuredClone(loadHilCatalog());
  unofficial.targets[0].board.datasheetUrl =
    "https://example.com/stm32f446re.pdf";
  assert.throws(
    () => validateHilCatalog(unofficial),
    /approved HTTPS domain/u,
  );

  const arbitraryCommand = structuredClone(loadHilCatalog());
  arbitraryCommand.targets[1].probe.versionCommand = ["rm", "--version"];
  assert.throws(
    () => validateHilCatalog(arbitraryCommand),
    /versionCommand is not approved/u,
  );
});

test("HIL CLI selects exact targets and rejects unknown input", () => {
  assert.deepEqual(
    parseHilArgs([
      "--target",
      "stm32-nucleo-f446re,nxp-frdm-mcxn947",
      "--require-tools",
    ]),
    {
      help: false,
      probeTools: true,
      requireTools: true,
      targetIds: ["stm32-nucleo-f446re", "nxp-frdm-mcxn947"],
    },
  );
  assert.throws(
    () => parseHilArgs(["--target", "unknown"]),
    /Unknown HIL target/u,
  );
  assert.throws(
    () => selectHilTargets(loadHilCatalog(), [
      "stm32-nucleo-f446re",
      "stm32-nucleo-f446re",
    ]),
    /duplicates/u,
  );
});

test("missing optional HIL dependencies are skipped or required", () => {
  const missingExecutable = () => ({
    error: Object.assign(new Error("missing"), { code: "ENOENT" }),
  });
  const options = {
    env: {},
    log: () => {},
    pathExists: () => false,
    spawn: missingExecutable,
    targetIds: ["stm32-nucleo-f446re"],
  };
  const summary = checkHilReadiness(loadHilCatalog(), options);

  assert.deepEqual(summary.readyTargets, []);
  assert.deepEqual(summary.skippedTargets, ["stm32-nucleo-f446re"]);
  assert.equal(summary.dependencies.length, 3);
  assert.ok(summary.dependencies.every((dependency) =>
    !dependency.available));
  assert.throws(
    () => checkHilReadiness(loadHilCatalog(), {
      ...options,
      requireTools: true,
    }),
    /dependencies are required/u,
  );
});

test("HIL readiness uses fixed argv without a shell", () => {
  const calls = [];
  const fakeSpawn = (command, args, options) => {
    calls.push({ args, command, options });
    return {
      status: 0,
      stderr: "",
      stdout: `${command} 1.0\n`,
    };
  };
  const summary = checkHilReadiness(loadHilCatalog(), {
    env: { HIL_MCUXPRESSO_SDK_ROOT: "/opt/mcuxpresso" },
    log: () => {},
    pathExists: (path) => path === "/opt/mcuxpresso",
    spawn: fakeSpawn,
    targetIds: ["nxp-frdm-mcxn947"],
  });

  assert.deepEqual(summary.readyTargets, ["nxp-frdm-mcxn947"]);
  assert.deepEqual(summary.skippedTargets, []);
  assert.equal(calls.length, 2);
  assert.deepEqual(
    calls.map((call) => [call.command, ...call.args]),
    [
      ["arm-none-eabi-gcc", "--version"],
      ["LinkServer", "--version"],
    ],
  );
  assert.ok(calls.every((call) => call.options.shell === false));
  assert.ok(calls.every((call) => call.options.timeout === 10000));
});

test("HIL reports pin catalog, firmware, dependencies, and all outcomes", () => {
  const catalog = loadHilCatalog();
  const report = validHilReport(catalog);

  assert.equal(validateHilReport(report, catalog), report);

  const wrongCatalog = structuredClone(report);
  wrongCatalog.catalog.sha256 = "f".repeat(64);
  assert.throws(
    () => validateHilReport(wrongCatalog, catalog),
    /catalog reference does not match/u,
  );

  const wrongOutcome = structuredClone(report);
  wrongOutcome.tests[3].status = "fail";
  assert.throws(
    () => validateHilReport(wrongOutcome, catalog),
    /success does not match test outcomes/u,
  );

  const rawProbeSerial = structuredClone(report);
  rawProbeSerial.board.probeSerialSha256 = "066EFF123456";
  assert.throws(
    () => validateHilReport(rawProbeSerial, catalog),
    /probeSerialSha256 is invalid/u,
  );

  const pathBearingVersion = structuredClone(report);
  pathBearingVersion.dependencies[0].version = "/opt/vendor/tool 2.0";
  assert.throws(
    () => validateHilReport(pathBearingVersion, catalog),
    /dependency 0.version is invalid/u,
  );
});

test("HIL JSON Schema declares a strict versioned contract", () => {
  const schema = JSON.parse(
    readFileSync(
      new URL("../schemas/hil-targets.schema.json", import.meta.url),
      "utf8",
    ),
  );

  assert.equal(schema.$schema, "https://json-schema.org/draft/2020-12/schema");
  assert.equal(schema.additionalProperties, false);
  assert.equal(schema.properties.schemaVersion.const, "1.0");
  assert.equal(schema.properties.targets.minItems, 3);
  assert.equal(schema.properties.targets.maxItems, 3);
  assert.equal(schema.$defs.target.additionalProperties, false);

  const reportSchema = JSON.parse(
    readFileSync(
      new URL(
        "../schemas/hil-validation-report.schema.json",
        import.meta.url,
      ),
      "utf8",
    ),
  );
  assert.equal(reportSchema.additionalProperties, false);
  assert.equal(reportSchema.properties.schemaVersion.const, "1.0");
  assert.equal(reportSchema.properties.tests.minItems, 6);
  assert.equal(reportSchema.properties.tests.maxItems, 6);
});

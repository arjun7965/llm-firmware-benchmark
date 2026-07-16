import {
  copyFileSync,
  cpSync,
  mkdirSync,
  mkdtempSync,
  readFileSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { runFixtureValidation } from "../src/fixture-sandbox.mjs";
import {
  fixtureAnswerFiles,
  validateFixtureRepository,
} from "../src/fixtures.mjs";
import {
  environmentFingerprint,
  getValidationEnvironmentRevision,
  getValidationProfile,
  profileFingerprint,
} from "../src/validation-profiles.mjs";

const repositoryRoot = fileURLToPath(new URL("../", import.meta.url));
const repositoryFixtures = join(repositoryRoot, "fixtures");
const repositoryTasks = join(repositoryRoot, "tasks.json");
const temporaryRoot = mkdtempSync(
  join(tmpdir(), "fixture-sandbox-self-test-"),
);
const temporaryFixtures = join(temporaryRoot, "fixtures");
const temporaryTasks = join(temporaryRoot, "tasks.json");
const references = [
  {
    taskId: "bare-metal-timer",
    source: "reference/fictional_timer.c",
    suite: "firmware",
  },
  {
    taskId: "interrupt-vector-configuration",
    source: "reference/interrupt_vector.c",
    suite: "firmware",
  },
  {
    taskId: "i2c-controller-recovery",
    source: "reference/i2c_controller.c",
    suite: "firmware",
  },
  {
    taskId: "gpio-edge-debounce",
    source: "reference/gpio_debounce.c",
    suite: "firmware",
  },
  {
    taskId: "adc-threshold-watchdog",
    source: "reference/adc_watchdog.c",
    suite: "firmware",
  },
  {
    taskId: "pwm-synchronized-update",
    source: "reference/pwm_update.c",
    suite: "firmware",
  },
  {
    taskId: "watchdog-window-recovery",
    source: "reference/watchdog_window.c",
    suite: "firmware",
  },
  {
    taskId: "timer-dma-handoff",
    source: "reference/timer_dma_handoff.c",
    suite: "firmware",
  },
  {
    taskId: "uart-interrupt-driver",
    source: "reference/uart_driver.c",
    suite: "firmware",
  },
  {
    taskId: "spi-dma-transfer",
    source: "reference/spi_dma_driver.c",
    suite: "firmware",
  },
  {
    taskId: "binary-parser",
    source: "reference/binary_parser.c",
    suite: "firmware",
  },
  {
    taskId: "concurrency-debug",
    source: "reference/pool.py",
    suite: "auxiliary",
    artifactCount: 0,
  },
  {
    taskId: "embedded-ring-buffer",
    source: "reference/ring_buffer.c",
    suite: "firmware",
  },
  {
    taskId: "firmware-state-machine",
    source: "reference/firmware_state_machine.c",
    suite: "firmware",
  },
  {
    taskId: "rtos-priority-inversion",
    source: "reference/priority_inversion.c",
    suite: "firmware",
  },
  {
    taskId: "frontend-autocomplete",
    source: "reference/Autocomplete.tsx",
    suite: "auxiliary",
    artifactCount: 0,
  },
  {
    taskId: "go-graceful-shutdown",
    suite: "auxiliary",
  },
  {
    taskId: "postgres-pagination",
    suite: "auxiliary",
    artifactCount: 0,
  },
  {
    taskId: "rust-stream-decoder",
    source: "reference/stream_decoder.rs",
    suite: "auxiliary",
  },
  {
    taskId: "testing-property-based",
    source: "reference/test_normalize_path.py",
    suite: "auxiliary",
    artifactCount: 0,
  },
  {
    taskId: "typescript-singleflight-cache",
    source: "reference/cache.ts",
    suite: "auxiliary",
    artifactCount: 0,
  },
];

try {
  validateFixtureRepository({
    fixturesRoot: repositoryFixtures,
    tasksPath: repositoryTasks,
  });
  mkdirSync(temporaryFixtures);
  copyFileSync(repositoryTasks, temporaryTasks);

  for (const reference of references) {
    const sourceRoot = join(repositoryFixtures, reference.taskId);
    const fixtureRoot = join(temporaryFixtures, reference.taskId);
    cpSync(sourceRoot, fixtureRoot, {
      recursive: true,
      filter: (source) =>
        !["build", "generated"].includes(source.split("/").at(-1)),
    });
    const manifest = JSON.parse(
      readFileSync(join(fixtureRoot, "manifest.json"), "utf8"),
    );
    const answerFiles = fixtureAnswerFiles(manifest);
    for (let index = 0; index < answerFiles.length; index++) {
      const answerPath = join(fixtureRoot, answerFiles[index].path);
      mkdirSync(dirname(answerPath), { recursive: true });
      const source = manifest.answer.format === "markdown-file-bundle"
        ? join(
          sourceRoot,
          manifest.paths.reference,
          manifest.answer.files[index].path,
        )
        : join(sourceRoot, reference.source);
      copyFileSync(source, answerPath);
    }

    const validationProfile = getValidationProfile(
      manifest.validationProfile,
    );
    const environmentReference = validationProfile.environments[0];
    const validationEnvironment = getValidationEnvironmentRevision(
      environmentReference.id,
      environmentReference.revision,
    );

    const { report } = runFixtureValidation({
      taskId: reference.taskId,
      fixturesRoot: temporaryFixtures,
      tasksPath: temporaryTasks,
    });
    if (!report.success) {
      throw new Error(
        `${reference.taskId} sandbox validation failed: ` +
        JSON.stringify(report.phases),
      );
    }
    if (
      report.schemaVersion !== "1.6" ||
      report.suite !== reference.suite ||
      report.validationProfile !== validationProfile.id ||
      report.validationProfileRevision !== validationProfile.revision ||
      report.validationProfileSha256 !==
        profileFingerprint(validationProfile) ||
      report.validationEnvironment.id !== validationEnvironment.id ||
      report.validationEnvironment.revision !==
        validationEnvironment.revision ||
      report.validationEnvironment.sha256 !==
        environmentFingerprint(validationEnvironment) ||
      report.validationEnvironment.host.operatingSystem !==
        validationEnvironment.host.operatingSystem ||
      report.validationEnvironment.host.release !==
        validationEnvironment.host.release ||
      report.validationEnvironment.host.architecture !==
        validationEnvironment.host.architecture ||
      report.validationEnvironment.execution.kind !==
        validationEnvironment.execution.kind ||
      report.toolchains.length !== validationProfile.toolchains.length ||
      report.answerFiles.length !== answerFiles.length ||
      report.toolchains.map((toolchain) => toolchain.name).join(",") !==
        [...validationProfile.toolchains].sort().join(",") ||
      report.toolchains.some((toolchain) =>
        toolchain.version === "" ||
        toolchain.versionArgv.join(" ") !==
          [
            toolchain.executable,
            ...manifest.toolVersionArgs[toolchain.name],
          ].join(" ")) ||
      report.artifacts.length !== (reference.artifactCount ?? 1) ||
      report.artifacts.some((artifact) => artifact.sizeBytes < 1) ||
      report.phases.some((phase) => phase.outcome !== "passed")
    ) {
      throw new Error(
        `${reference.taskId} reproducibility metadata is incomplete`,
      );
    }
    console.log(`ok - ${reference.taskId}`);
  }

  const probeRoot = join(temporaryFixtures, "binary-parser");
  const probeManifestPath = join(probeRoot, "manifest.json");
  const probeManifest = JSON.parse(
    readFileSync(probeManifestPath, "utf8"),
  );
  probeManifest.commands = [
    {
      id: "sandbox-probe-compile",
      phase: "compile",
      argv: [
        "cc",
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-pedantic",
        "generated/answer.c",
        "-o",
        "build/public-tests",
      ],
      requiredTools: ["cc"],
      timeoutMs: 30_000,
    },
    {
      id: "sandbox-probe-test",
      phase: "test",
      argv: ["build/public-tests"],
      requiredTools: [],
      timeoutMs: 5_000,
    },
  ];
  writeFileSync(
    probeManifestPath,
    `${JSON.stringify(probeManifest, null, 2)}\n`,
  );
  writeFileSync(
    join(probeRoot, "generated", "answer.c"),
    [
      "#include <stdio.h>",
      "",
      "int main(void) {",
      "  FILE *rootFile = fopen(\"/workspace/unexpected-write\", \"w\");",
      "  if (rootFile != NULL) {",
      "    (void)fclose(rootFile);",
      "    return 10;",
      "  }",
      "  FILE *temporaryFile = fopen(\"/tmp/expected-write\", \"w\");",
      "  if (temporaryFile == NULL) return 11;",
      "  return fclose(temporaryFile) == 0 ? 0 : 12;",
      "}",
      "",
    ].join("\n"),
  );
  const { report: probeReport } = runFixtureValidation({
    taskId: "binary-parser",
    fixturesRoot: temporaryFixtures,
    tasksPath: temporaryTasks,
  });
  if (!probeReport.success) {
    throw new Error(
      "sandbox filesystem probe failed: " +
      JSON.stringify(probeReport.phases),
    );
  }
  console.log("ok - root filesystem is read-only and /tmp is writable");
  console.log("Trusted fixture sandbox validation passed.");
} finally {
  rmSync(temporaryRoot, { recursive: true, force: true });
}

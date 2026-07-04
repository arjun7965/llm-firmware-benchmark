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
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { runFixtureValidation } from "../src/fixture-sandbox.mjs";
import { validateFixtureRepository } from "../src/fixtures.mjs";

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
  },
  {
    taskId: "binary-parser",
    source: "reference/binary_parser.c",
  },
  {
    taskId: "embedded-ring-buffer",
    source: "reference/ring_buffer.c",
  },
  {
    taskId: "firmware-state-machine",
    source: "reference/firmware_state_machine.c",
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
    mkdirSync(join(fixtureRoot, "generated"));
    copyFileSync(
      join(sourceRoot, reference.source),
      join(fixtureRoot, "generated", "answer.c"),
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
      report.schemaVersion !== "1.2" ||
      report.suite !== "firmware" ||
      report.toolchains.length !== 1 ||
      report.toolchains[0].name !== "cc" ||
      report.toolchains[0].version === "" ||
      report.toolchains[0].versionArgv.join(" ") !==
        `${report.toolchains[0].executable} --version` ||
      report.artifacts.length !== 1 ||
      report.artifacts[0].sizeBytes < 1 ||
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

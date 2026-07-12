import { spawnSync } from "node:child_process";
import {
  mkdirSync,
  mkdtempSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { runFixtureMutationTests } from "../../../src/fixture-mutations.mjs";

const repositoryRoot = fileURLToPath(new URL("../../../", import.meta.url));
const fixtureRoot = join(repositoryRoot, "fixtures", "go-graceful-shutdown");

function verifySourcePolicy() {
  const temporaryRoot = mkdtempSync(join(tmpdir(), "go-source-policy-"));
  const buildRoot = join(temporaryRoot, "build");
  mkdirSync(buildRoot);
  const candidatePath = join(temporaryRoot, "answer.go");
  writeFileSync(candidatePath, [
    "package shutdown",
    "",
    "import \"os\"",
    "",
    "func init() { os.Exit(0) }",
    "",
  ].join("\n"));

  try {
    const result = spawnSync("go", [
      "run",
      "starter/build_tests.go",
      "--answer",
      candidatePath,
      "--candidate-tests",
      "reference/server_test.go",
      "--tests",
      "tests/public/server_test.go",
      "--supervisor",
      "starter/test_supervisor.go",
      "--module-root",
      join(buildRoot, "module"),
      "--test-binary",
      join(buildRoot, "candidate-tests"),
      "--validator-test-binary",
      join(buildRoot, "validator-tests"),
      "--output",
      join(buildRoot, "public-tests"),
    ], {
      cwd: fixtureRoot,
      encoding: "utf8",
      stdio: ["ignore", "pipe", "pipe"],
      timeout: 30_000,
    });
    const diagnostics = `${result.stdout ?? ""}\n${result.stderr ?? ""}`;
    if (result.error || result.status === 0 ||
        !diagnostics.includes("init functions are not allowed")) {
      throw new Error(
        "Go source policy did not reject initialization-time exit",
      );
    }
  } finally {
    rmSync(temporaryRoot, { recursive: true, force: true });
  }
}

verifySourcePolicy();
const result = runFixtureMutationTests({
  fixtureStatuses: ["active"],
  fixturesRoot: join(repositoryRoot, "fixtures"),
  taskIds: ["go-graceful-shutdown"],
  tasksPath: join(repositoryRoot, "tasks.json"),
});

if (result.fixtureCount !== 1 || result.killedMutations !== 22) {
  throw new Error("Go graceful-shutdown calibration is incomplete");
}

console.log("Go graceful-shutdown trusted reference and mutations passed.");

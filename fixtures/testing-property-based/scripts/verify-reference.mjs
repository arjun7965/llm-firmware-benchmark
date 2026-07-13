import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { runFixtureMutationTests } from
  "../../../src/fixture-mutations.mjs";

const repositoryRoot = fileURLToPath(new URL("../../../", import.meta.url));
const hypothesisStorage = mkdtempSync(join(tmpdir(), "property-hypothesis-"));
const previousStorage = process.env.HYPOTHESIS_STORAGE_DIRECTORY;
const previousBytecode = process.env.PYTHONDONTWRITEBYTECODE;
process.env.HYPOTHESIS_STORAGE_DIRECTORY = hypothesisStorage;
process.env.PYTHONDONTWRITEBYTECODE = "1";

let result;
try {
  result = runFixtureMutationTests({
    fixtureStatuses: ["active"],
    fixturesRoot: join(repositoryRoot, "fixtures"),
    taskIds: ["testing-property-based"],
    tasksPath: join(repositoryRoot, "tasks.json"),
  });
} finally {
  if (previousStorage === undefined) {
    delete process.env.HYPOTHESIS_STORAGE_DIRECTORY;
  } else {
    process.env.HYPOTHESIS_STORAGE_DIRECTORY = previousStorage;
  }
  if (previousBytecode === undefined) {
    delete process.env.PYTHONDONTWRITEBYTECODE;
  } else {
    process.env.PYTHONDONTWRITEBYTECODE = previousBytecode;
  }
  rmSync(hypothesisStorage, { recursive: true, force: true });
}

if (result.fixtureCount !== 1 || result.killedMutations !== 12) {
  throw new Error("Property-based testing calibration is incomplete");
}

console.log("Property-based testing trusted answer and mutations passed.");

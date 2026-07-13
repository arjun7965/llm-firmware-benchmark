import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { runFixtureMutationTests } from "../../../src/fixture-mutations.mjs";

const repositoryRoot = fileURLToPath(new URL("../../../", import.meta.url));
const result = runFixtureMutationTests({
  fixtureStatuses: ["active"],
  fixturesRoot: join(repositoryRoot, "fixtures"),
  taskIds: ["frontend-autocomplete"],
  tasksPath: join(repositoryRoot, "tasks.json"),
});

if (result.fixtureCount !== 1 || result.killedMutations !== 16) {
  throw new Error("Frontend autocomplete calibration is incomplete");
}

console.log("Frontend autocomplete reference and mutations passed.");

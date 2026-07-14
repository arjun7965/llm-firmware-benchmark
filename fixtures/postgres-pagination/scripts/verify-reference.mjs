import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { runFixtureMutationTests } from "../../../src/fixture-mutations.mjs";

const repositoryRoot = fileURLToPath(new URL("../../../", import.meta.url));
const result = runFixtureMutationTests({
  fixtureStatuses: ["active"],
  fixturesRoot: join(repositoryRoot, "fixtures"),
  taskIds: ["postgres-pagination"],
  tasksPath: join(repositoryRoot, "tasks.json"),
});

if (result.fixtureCount !== 1 || result.killedMutations !== 12) {
  throw new Error("PostgreSQL pagination calibration is incomplete");
}

console.log("PostgreSQL pagination reference and mutations passed.");

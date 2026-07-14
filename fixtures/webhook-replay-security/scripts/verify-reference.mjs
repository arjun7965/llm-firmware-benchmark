import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { runFixtureMutationTests } from "../../../src/fixture-mutations.mjs";

const repositoryRoot = fileURLToPath(new URL("../../../", import.meta.url));
const result = runFixtureMutationTests({
  fixtureStatuses: ["active"],
  fixturesRoot: join(repositoryRoot, "fixtures"),
  taskIds: ["webhook-replay-security"],
  tasksPath: join(repositoryRoot, "tasks.json"),
});

if (result.fixtureCount !== 1 || result.killedMutations !== 11) {
  throw new Error("Webhook replay security calibration is incomplete");
}

console.log("Webhook replay security reference and mutations passed.");

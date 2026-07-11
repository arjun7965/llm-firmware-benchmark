import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { runFixtureMutationTests } from "../../../src/fixture-mutations.mjs";

const repositoryRoot = fileURLToPath(new URL("../../../", import.meta.url));
const result = runFixtureMutationTests({
  fixtureStatuses: ["active"],
  fixturesRoot: join(repositoryRoot, "fixtures"),
  taskIds: ["rust-stream-decoder"],
  tasksPath: join(repositoryRoot, "tasks.json"),
});

if (result.fixtureCount !== 1 || result.killedMutations !== 8) {
  throw new Error("Rust stream-decoder calibration is incomplete");
}

console.log("Rust stream-decoder trusted reference and mutations passed.");

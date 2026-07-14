import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { runFixtureMutationTests } from "../../../src/fixture-mutations.mjs";
import { runLocalPostgresqlCommand } from "../../../src/postgresql-service.mjs";
import { getValidationProfile } from "../../../src/validation-profiles.mjs";

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

const blockedSuperuserLogin = runLocalPostgresqlCommand({
  args: [
    "-X",
    "-v",
    "ON_ERROR_STOP=1",
    "--username=validator",
    "-Atqc",
    "SELECT 1",
  ],
  command: "psql",
  cwd: join(repositoryRoot, "fixtures", "postgres-pagination"),
  profile: getValidationProfile("postgresql"),
  timeoutMs: 10_000,
});
if (
  blockedSuperuserLogin.status === 0 ||
  !blockedSuperuserLogin.stderr.includes("not permitted to log in")
) {
  throw new Error("PostgreSQL bootstrap superuser still accepts logins");
}

console.log("PostgreSQL pagination reference and mutations passed.");

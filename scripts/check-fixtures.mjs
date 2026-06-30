import { fileURLToPath } from "node:url";
import { validateFixtureRepository } from "../src/fixtures.mjs";

const summary = validateFixtureRepository({
  fixturesRoot: fileURLToPath(new URL("../fixtures/", import.meta.url)),
  tasksPath: new URL("../tasks.json", import.meta.url),
});

console.log(`Fixture validation passed: ${JSON.stringify(summary)}`);

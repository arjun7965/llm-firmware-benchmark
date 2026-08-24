import { parseArgs } from "node:util";
import { fileURLToPath } from "node:url";
import { join } from "node:path";
import { runOciC11Calibration } from "../src/oci-calibration.mjs";

const repositoryRoot = fileURLToPath(new URL("../", import.meta.url));
const { values } = parseArgs({
  strict: true,
  options: {
    "shard-count": { type: "string", default: "1" },
    "shard-index": { type: "string", default: "0" },
  },
});
const shardCount = Number(values["shard-count"]);
const shardIndex = Number(values["shard-index"]);

runOciC11Calibration({
  fixturesRoot: join(repositoryRoot, "fixtures"),
  shardCount,
  shardIndex,
  tasksPath: join(repositoryRoot, "tasks.json"),
});

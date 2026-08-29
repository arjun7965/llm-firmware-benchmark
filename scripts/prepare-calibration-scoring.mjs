import {
  existsSync,
  mkdirSync,
  readFileSync,
  writeFileSync,
} from "node:fs";
import {
  resolve,
} from "node:path";
import { parseArgs } from "node:util";
import {
  buildBlindedScoringArtifacts,
  loadCalibrationSamples,
  resolvePrivateResultsPath,
} from "../src/calibration-scoring.mjs";
import { loadTasks } from "../src/harness.mjs";

const { values } = parseArgs({
  allowPositionals: false,
  options: {
    input: { type: "string" },
    output: { type: "string" },
    rubric: { type: "string" },
    task: { type: "string" },
    tasks: { type: "string", default: "tasks.json" },
  },
  strict: true,
});

function requiredOption(value, name) {
  if (typeof value !== "string" || value.trim() === "") {
    throw new TypeError(`--${name} is required`);
  }
  return value;
}

const taskId = requiredOption(values.task, "task");
const input = resolvePrivateResultsPath(
  requiredOption(values.input, "input"),
  { name: "--input" },
);
const output = resolvePrivateResultsPath(
  requiredOption(values.output, "output"),
  { mustExist: false, name: "--output" },
);
if (existsSync(output)) {
  throw new TypeError(`refusing to overwrite calibration scoring output: ${output}`);
}

const tasks = loadTasks(resolve(values.tasks));
const task = tasks.find((candidate) => candidate.id === taskId);
if (task === undefined) {
  throw new TypeError(`unknown task: ${taskId}`);
}
const rubricPath = resolve(
  values.rubric ?? `docs/benchmarks/${taskId}.md`,
);
const rubric = readFileSync(rubricPath, "utf8");
const samples = loadCalibrationSamples(input, task);
const artifacts = buildBlindedScoringArtifacts({ rubric, samples, task });

mkdirSync(output, { mode: 0o700 });
writeFileSync(resolve(output, "packet.json"), artifacts.packetText, {
  flag: "wx",
  mode: 0o600,
});
writeFileSync(resolve(output, "packet.md"), artifacts.packetMarkdownText, {
  flag: "wx",
  mode: 0o600,
});
writeFileSync(resolve(output, "score-sheet.json"), artifacts.scoreSheetText, {
  flag: "wx",
  mode: 0o600,
});
writeFileSync(resolve(output, "identity-key.json"), artifacts.keyText, {
  flag: "wx",
  mode: 0o600,
});

console.log(`Prepared ${samples.length} blinded samples in ${output}`);
console.log("Review packet.md and complete score-sheet.json before opening identity-key.json.");

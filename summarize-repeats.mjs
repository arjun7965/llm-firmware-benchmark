import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { summarizeModelScores } from "./src/statistics.mjs";

const scoresPath = process.env.BENCHMARK_SCORES_FILE
  ? resolve(process.env.BENCHMARK_SCORES_FILE)
  : new URL("./repeat-scores.json", import.meta.url);
const scores = JSON.parse(
  readFileSync(scoresPath, "utf8"),
);

const reservedKeys = new Set(["rubric", "tasks"]);
const models = Object.keys(scores).filter((key) => !reservedKeys.has(key));
for (const model of models) {
  console.log(JSON.stringify(summarizeModelScores(scores, model), null, 2));
}

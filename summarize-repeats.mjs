import { resolve } from "node:path";
import { loadScores, scoreModelIds } from "./src/scores.mjs";
import { summarizeModelScores } from "./src/statistics.mjs";

const scoresPath = process.env.BENCHMARK_SCORES_FILE
  ? resolve(process.env.BENCHMARK_SCORES_FILE)
  : new URL("./repeat-scores.json", import.meta.url);
const scores = loadScores(scoresPath);

for (const model of scoreModelIds(scores)) {
  console.log(JSON.stringify(summarizeModelScores(scores, model), null, 2));
}

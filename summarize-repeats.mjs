import { resolve } from "node:path";
import { loadTasks } from "./src/harness.mjs";
import { loadScores, scoreModelIds } from "./src/scores.mjs";
import { summarizeModelScores } from "./src/statistics.mjs";

const scoresPath = process.env.BENCHMARK_SCORES_FILE
  ? resolve(process.env.BENCHMARK_SCORES_FILE)
  : new URL("./repeat-scores.json", import.meta.url);
const tasksPath = process.env.BENCHMARK_TASKS_FILE
  ? resolve(process.env.BENCHMARK_TASKS_FILE)
  : new URL("./tasks.json", import.meta.url);
const tasks = loadTasks(tasksPath);
const scores = loadScores(scoresPath, {
  validationProfileByTask: new Map(
    tasks.map((task) => [task.id, task.validationProfile]),
  ),
});
const suiteByTask = new Map(
  tasks.map((task) => [task.id, task.suite]),
);

for (const model of scoreModelIds(scores)) {
  console.log(JSON.stringify(summarizeModelScores(scores, model, {
    suiteByTask,
  }), null, 2));
}

import { fileURLToPath } from "node:url";
import {
  createJobs,
  executeJob,
  loadTasks,
  mapWithConcurrency,
} from "./src/harness.mjs";
import { loadModels } from "./src/models.mjs";
import { generateWithProvider } from "./src/providers/index.mjs";

const tasks = loadTasks(new URL("./tasks.json", import.meta.url));
const modelsPath = process.env.BENCHMARK_MODELS_FILE ??
  fileURLToPath(new URL("./models.local.json", import.meta.url));
const models = loadModels(modelsPath);
const jobs = createJobs(tasks, models, [2, 3]);
const outputRoot = fileURLToPath(new URL("./results/", import.meta.url));

await mapWithConcurrency(jobs, 4, async (job) => {
  const result = await executeJob({
    job,
    outputRoot,
    generate: generateWithProvider,
  });
  if (result.status === "skipped") {
    console.log(`run-${job.run} ${job.task.id} ${job.modelName}: skipped`);
  } else {
    const { exitCode, signal } = result.record;
    console.log(
      `run-${job.run} ${job.task.id} ${job.modelName}: ` +
      `exit=${exitCode} signal=${signal ?? "none"}`,
    );
  }
});

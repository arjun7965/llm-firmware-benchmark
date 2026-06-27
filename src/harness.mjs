import {
  existsSync,
  mkdirSync,
  readFileSync,
  writeFileSync,
} from "node:fs";
import { dirname, join } from "node:path";
import { validateModels } from "./models.mjs";

const taskIdPattern = /^[a-z0-9]+(?:-[a-z0-9]+)*$/;

export function validateTasks(tasks) {
  if (!Array.isArray(tasks) || tasks.length === 0) {
    throw new TypeError("tasks must be a non-empty array");
  }

  const ids = new Set();
  for (const [index, task] of tasks.entries()) {
    if (!task || typeof task !== "object" || Array.isArray(task)) {
      throw new TypeError(`task at index ${index} must be an object`);
    }
    if (typeof task.id !== "string" || !taskIdPattern.test(task.id)) {
      throw new TypeError(`task at index ${index} has an invalid id`);
    }
    if (ids.has(task.id)) {
      throw new TypeError(`duplicate task id: ${task.id}`);
    }
    if (typeof task.category !== "string" || task.category.trim() === "") {
      throw new TypeError(`task ${task.id} must have a category`);
    }
    if (typeof task.prompt !== "string" || task.prompt.trim() === "") {
      throw new TypeError(`task ${task.id} must have a prompt`);
    }
    ids.add(task.id);
  }

  return tasks;
}

export function loadTasks(path) {
  return validateTasks(JSON.parse(readFileSync(path, "utf8")));
}

export function createJobs(tasks, models, runs = [1]) {
  validateTasks(tasks);
  validateModels(models);
  if (!Array.isArray(runs) || runs.length === 0 ||
      runs.some((run) => !Number.isInteger(run) || run < 1)) {
    throw new TypeError("runs must contain positive integers");
  }

  return runs.flatMap((run) =>
    tasks.flatMap((task) =>
      models.map((model) => ({
        run,
        task,
        provider: model.provider,
        modelName: model.id,
        modelId: model.model,
        modelOptions: model.options ?? {},
      })),
    ),
  );
}

export function resultFilePath(outputRoot, job) {
  const outputDir = job.run === 1
    ? outputRoot
    : join(outputRoot, `run-${job.run}`);
  return join(outputDir, `${job.task.id}--${job.modelName}.json`);
}

export function hasSuccessfulResult(path) {
  if (!existsSync(path)) return false;
  try {
    return JSON.parse(readFileSync(path, "utf8")).exitCode === 0;
  } catch {
    return false;
  }
}

export async function mapWithConcurrency(items, concurrency, handler) {
  if (!Number.isInteger(concurrency) || concurrency < 1) {
    throw new RangeError("concurrency must be a positive integer");
  }
  if (typeof handler !== "function") {
    throw new TypeError("handler must be a function");
  }

  const results = new Array(items.length);
  let nextIndex = 0;
  async function worker() {
    while (nextIndex < items.length) {
      const index = nextIndex++;
      results[index] = await handler(items[index], index);
    }
  }

  const workerCount = Math.min(concurrency, items.length);
  await Promise.all(Array.from({ length: workerCount }, worker));
  return results;
}

export async function executeJob({
  job,
  outputRoot,
  generate,
  now = () => new Date(),
}) {
  if (typeof generate !== "function") {
    throw new TypeError("generate must be a function");
  }

  const path = resultFilePath(outputRoot, job);
  if (hasSuccessfulResult(path)) {
    return { status: "skipped", path };
  }

  mkdirSync(dirname(path), { recursive: true });
  const startedAt = now().toISOString();
  let outcome;
  try {
    outcome = await generate(job);
  } catch (error) {
    outcome = {
      exitCode: null,
      signal: null,
      stdout: "",
      stderr: "",
      error: error.message,
    };
  }

  const record = {
    run: job.run,
    task: job.task.id,
    category: job.task.category,
    provider: job.provider,
    modelName: job.modelName,
    modelId: job.modelId,
    modelOptions: job.modelOptions,
    startedAt,
    finishedAt: now().toISOString(),
    exitCode: outcome.exitCode ?? null,
    signal: outcome.signal ?? null,
    stdout: outcome.stdout ?? "",
    stderr: outcome.stderr ?? "",
    error: outcome.error ?? null,
  };
  writeFileSync(path, JSON.stringify(record, null, 2));
  return { status: "completed", path, record };
}

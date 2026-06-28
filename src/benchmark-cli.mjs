import { fileURLToPath } from "node:url";
import { resolve } from "node:path";
import { parseArgs } from "node:util";
import {
  createJobs,
  executeJob,
  loadTasks,
  mapWithConcurrency,
} from "./harness.mjs";
import { loadModels } from "./models.mjs";
import { generateWithProvider } from "./providers/index.mjs";

const repositoryTasksPath = fileURLToPath(
  new URL("../tasks.json", import.meta.url),
);
const repositoryModelsPath = fileURLToPath(
  new URL("../models.local.json", import.meta.url),
);
const repositoryOutputPath = fileURLToPath(
  new URL("../results/", import.meta.url),
);

function parseList(values, name) {
  if (values === undefined) return null;
  const items = values
    .flatMap((value) => value.split(","))
    .map((value) => value.trim());
  if (items.length === 0 || items.some((value) => value === "")) {
    throw new TypeError(`${name} must contain non-empty comma-separated values`);
  }
  if (new Set(items).size !== items.length) {
    throw new TypeError(`${name} cannot contain duplicates`);
  }
  return items;
}

function parsePositiveInteger(value, name) {
  if (!/^[1-9][0-9]*$/.test(value)) {
    throw new TypeError(`${name} must be a positive integer`);
  }
  const parsed = Number(value);
  if (!Number.isSafeInteger(parsed)) {
    throw new TypeError(`${name} must be a safe positive integer`);
  }
  return parsed;
}

function parseRuns(values, defaults) {
  const entries = parseList(values, "runs");
  if (entries === null) return [...defaults];
  return entries.map((value) => parsePositiveInteger(value, "runs"));
}

export function parseBenchmarkArgs(args, {
  cwd = process.cwd(),
  defaultRuns = [1],
  environment = process.env,
} = {}) {
  if (!Array.isArray(defaultRuns) || defaultRuns.length === 0 ||
      defaultRuns.some((run) => !Number.isInteger(run) || run < 1)) {
    throw new TypeError("defaultRuns must contain positive integers");
  }

  const { values } = parseArgs({
    args,
    allowPositionals: false,
    strict: true,
    options: {
      concurrency: { type: "string", short: "j", default: "4" },
      help: { type: "boolean", short: "h", default: false },
      models: { type: "string", short: "m", multiple: true },
      "models-file": { type: "string" },
      output: { type: "string", short: "o" },
      runs: { type: "string", short: "r", multiple: true },
      tasks: { type: "string", short: "t", multiple: true },
      "tasks-file": { type: "string" },
    },
  });

  const modelsFile = values["models-file"] ??
    environment.BENCHMARK_MODELS_FILE ??
    repositoryModelsPath;
  const tasksFile = values["tasks-file"] ?? repositoryTasksPath;
  const output = values.output ?? repositoryOutputPath;

  return {
    concurrency: parsePositiveInteger(values.concurrency, "concurrency"),
    help: values.help,
    modelIds: parseList(values.models, "models"),
    modelsFile: resolve(cwd, modelsFile),
    outputRoot: resolve(cwd, output),
    runs: parseRuns(values.runs, defaultRuns),
    taskIds: parseList(values.tasks, "tasks"),
    tasksFile: resolve(cwd, tasksFile),
  };
}

export function filterByIds(items, ids, label) {
  if (ids === null) return items;
  const byId = new Map(items.map((item) => [item.id, item]));
  const unknown = ids.filter((id) => !byId.has(id));
  if (unknown.length > 0) {
    throw new TypeError(`unknown ${label}: ${unknown.join(", ")}`);
  }
  return ids.map((id) => byId.get(id));
}

export function benchmarkHelp(defaultRuns = [1], commandName = "benchmark") {
  return [
    `Usage: npm run ${commandName} -- [options]`,
    "",
    "Options:",
    "  -m, --models <ids>       Model IDs, comma-separated or repeated",
    "  -t, --tasks <ids>        Task IDs, comma-separated or repeated",
    `  -r, --runs <numbers>     Run numbers (default: ${defaultRuns.join(",")})`,
    "  -j, --concurrency <n>    Maximum concurrent jobs (default: 4)",
    "  -o, --output <path>      Result directory (default: results)",
    "      --models-file <path> Model configuration file",
    "      --tasks-file <path>  Task definition file",
    "  -h, --help               Show this help",
  ].join("\n");
}

export async function runBenchmarkCli({
  args = process.argv.slice(2),
  commandName = "benchmark",
  defaultRuns = [1],
  environment = process.env,
  log = console.log,
} = {}) {
  const configuration = parseBenchmarkArgs(args, {
    defaultRuns,
    environment,
  });
  if (configuration.help) {
    log(benchmarkHelp(defaultRuns, commandName));
    return { status: "help", jobCount: 0 };
  }

  const tasks = filterByIds(
    loadTasks(configuration.tasksFile),
    configuration.taskIds,
    "task IDs",
  );
  const models = filterByIds(
    loadModels(configuration.modelsFile),
    configuration.modelIds,
    "model IDs",
  );
  const jobs = createJobs(tasks, models, configuration.runs);

  await mapWithConcurrency(
    jobs,
    configuration.concurrency,
    async (job) => {
      const result = await executeJob({
        job,
        outputRoot: configuration.outputRoot,
        generate: generateWithProvider,
      });
      const runPrefix = job.run === 1 ? "" : `run-${job.run} `;
      if (result.status === "skipped") {
        log(`${runPrefix}${job.task.id} ${job.modelName}: skipped`);
      } else {
        const { exitCode, signal } = result.record;
        log(
          `${runPrefix}${job.task.id} ${job.modelName}: ` +
          `exit=${exitCode} signal=${signal ?? "none"}`,
        );
      }
      return result;
    },
  );

  return {
    status: "completed",
    jobCount: jobs.length,
    configuration,
  };
}

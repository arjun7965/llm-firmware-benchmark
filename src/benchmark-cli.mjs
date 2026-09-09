import { fileURLToPath } from "node:url";
import { resolve } from "node:path";
import { parseArgs } from "node:util";
import {
  createJobs,
  executeJob,
  loadTasks,
  mapWithConcurrency,
} from "./harness.mjs";
import { loadModels, validateModels } from "./models.mjs";
import { validateCodexEffort } from "./providers/codex.mjs";
import { validateClaudeCodeEffort } from "./providers/claude-code.mjs";
import {
  generateWithProvider,
  getProviderConfigSha256,
} from "./providers/index.mjs";
import { suiteSet } from "./suites.mjs";

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

function parseReasoning(values = []) {
  const selections = new Map();
  for (const value of values) {
    const match = /^([a-z0-9]+(?:[._-][a-z0-9]+)*)=(.+)$/.exec(value);
    if (!match) throw new TypeError("reasoning must use model-id=level[,level]");
    const [, modelId, levelsText] = match;
    const levels = parseList([levelsText], "reasoning levels");
    if (levels.some((level) => !/^[a-z0-9]+(?:[-_][a-z0-9]+)*$/.test(level))) {
      throw new TypeError("reasoning levels must be lowercase labels");
    }
    const previous = selections.get(modelId) ?? [];
    if (levels.some((level) => previous.includes(level))) {
      throw new TypeError(`duplicate reasoning level for ${modelId}`);
    }
    selections.set(modelId, [...previous, ...levels]);
  }
  return [...selections].map(([modelId, levels]) => ({ modelId, levels }));
}

export function expandReasoningModels(models, selections, catalog = models) {
  const selectedIds = new Set(models.map((model) => model.id));
  const configuredIds = new Set(catalog.map((model) => model.id));
  for (const { modelId } of selections) {
    if (!selectedIds.has(modelId)) {
      throw new TypeError(`reasoning model is not selected: ${modelId}`);
    }
  }
  const levelsById = new Map(
    selections.map(({ modelId, levels }) => [modelId, levels]),
  );
  const expanded = models.flatMap((model) => {
    const levels = levelsById.get(model.id);
    if (!levels) return [model];
    return levels.map((level) => {
      const id = `${model.id}.reasoning-${level}`;
      if (configuredIds.has(id)) {
        throw new TypeError(`duplicate model id: ${id} conflicts with the model catalog`);
      }
      const options = { ...model.options };
      switch (model.provider) {
        case "codex":
          validateCodexEffort(level);
          options.effort = level;
          break;
        case "claude-code":
          validateClaudeCodeEffort(level);
          options.effort = level;
          break;
        case "opencode":
          options.variant = level;
          break;
        case "openai-compatible":
          if (options.request != null &&
              (typeof options.request !== "object" || Array.isArray(options.request))) {
            throw new TypeError("OpenAI-compatible request must be an object");
          }
          options.request = { ...options.request, reasoning_effort: level };
          break;
        default:
          throw new TypeError(`reasoning control is unsupported for ${model.provider}`);
      }
      return { ...model, id, options };
    });
  });
  return validateModels(expanded);
}

function parseSuites(values) {
  const suites = parseList(values, "suites");
  if (suites === null) return null;
  const unknown = suites.filter((suite) => !suiteSet.has(suite));
  if (unknown.length > 0) {
    throw new TypeError(`unknown suites: ${unknown.join(", ")}`);
  }
  return suites;
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
      reasoning: { type: "string", multiple: true },
      runs: { type: "string", short: "r", multiple: true },
      suites: { type: "string", short: "s", multiple: true },
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
    reasoning: parseReasoning(values.reasoning),
    runs: parseRuns(values.runs, defaultRuns),
    suiteIds: parseSuites(values.suites),
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

export function filterBySuites(tasks, suites) {
  if (suites === null) return tasks;
  const selected = tasks.filter((task) => suites.includes(task.suite));
  if (selected.length === 0) {
    throw new TypeError(`no tasks matched suites: ${suites.join(", ")}`);
  }
  return selected;
}

export function benchmarkHelp(defaultRuns = [1], commandName = "benchmark") {
  return [
    `Usage: npm run ${commandName} -- [options]`,
    "",
    "Options:",
    "  -m, --models <ids>       Model IDs, comma-separated or repeated",
    "  -s, --suites <names>     Suites: firmware, auxiliary",
    "  -t, --tasks <ids>        Task IDs, comma-separated or repeated",
    `  -r, --runs <numbers>     Run numbers (default: ${defaultRuns.join(",")})`,
    "  -j, --concurrency <n>    Maximum concurrent jobs (default: 4)",
    "  -o, --output <path>      Result directory (default: results)",
    "      --models-file <path> Model configuration file",
    "      --tasks-file <path>  Task definition file",
    "      --reasoning <id=levels> Per-model reasoning level(s), comma-separated; repeatable",
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

  const selectedTasks = filterByIds(
    loadTasks(configuration.tasksFile),
    configuration.taskIds,
    "task IDs",
  );
  const tasks = filterBySuites(selectedTasks, configuration.suiteIds);
  const catalog = loadModels(configuration.modelsFile);
  const selectedModels = filterByIds(
    catalog,
    configuration.modelIds,
    "model IDs",
  );
  const models = expandReasoningModels(selectedModels, configuration.reasoning, catalog);
  const jobs = createJobs(tasks, models, configuration.runs);

  await mapWithConcurrency(
    jobs,
    configuration.concurrency,
    async (job) => {
      const result = await executeJob({
        job,
        outputRoot: configuration.outputRoot,
        generate: generateWithProvider,
        providerConfigSha256: getProviderConfigSha256(job.provider),
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

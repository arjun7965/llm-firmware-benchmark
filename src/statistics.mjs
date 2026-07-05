import {
  requireSuite,
  suiteIds,
} from "./suites.mjs";

export function mean(values) {
  if (!Array.isArray(values) || values.length === 0 ||
      values.some((value) => !Number.isFinite(value))) {
    throw new TypeError("values must be a non-empty array of finite numbers");
  }
  return values.reduce((sum, value) => sum + value, 0) / values.length;
}

export function populationSd(values) {
  const average = mean(values);
  return Math.sqrt(mean(values.map((value) => (value - average) ** 2)));
}

function suiteForTask(suiteByTask, task) {
  let suite;
  if (suiteByTask instanceof Map) {
    suite = suiteByTask.get(task);
  } else if (
    suiteByTask &&
    typeof suiteByTask === "object" &&
    Object.hasOwn(suiteByTask, task)
  ) {
    suite = suiteByTask[task];
  }
  if (suite === undefined) {
    throw new TypeError(`missing suite for scored task: ${task}`);
  }
  return requireSuite(suite, `suite for scored task ${task}`);
}

function summarizeSuites(scores, runs, suiteByTask) {
  const indicesBySuite = new Map(
    suiteIds.map((suite) => [suite, []]),
  );
  for (const [index, task] of scores.tasks.entries()) {
    indicesBySuite.get(suiteForTask(suiteByTask, task)).push(index);
  }

  return suiteIds.flatMap((suite) => {
    const indices = indicesBySuite.get(suite);
    if (indices.length === 0) return [];
    const totals = runs.map((run) =>
      indices.reduce((sum, index) => sum + run[index], 0));
    return [{
      suite,
      tasks: indices.map((index) => scores.tasks[index]),
      totals,
      totalMean: mean(totals),
      totalSd: populationSd(totals),
      totalRange: Math.max(...totals) - Math.min(...totals),
    }];
  });
}

export function summarizeModelScores(scores, model, {
  suiteByTask,
} = {}) {
  if (!Array.isArray(scores.tasks) || scores.tasks.length === 0) {
    throw new TypeError("scores.tasks must be a non-empty array");
  }
  const modelRuns = scores[model];
  if (!modelRuns || typeof modelRuns !== "object" || Array.isArray(modelRuns)) {
    throw new TypeError(`missing scores for model: ${model}`);
  }

  const runEntries = Object.entries(modelRuns);
  if (runEntries.length === 0) {
    throw new TypeError(`model ${model} has no runs`);
  }
  for (const [runName, values] of runEntries) {
    if (!Array.isArray(values) || values.length !== scores.tasks.length ||
        values.some((value) => !Number.isFinite(value))) {
      throw new TypeError(`model ${model} has invalid scores for ${runName}`);
    }
  }

  const runs = runEntries.map(([, values]) => values);
  const totals = runs.map((run) => run.reduce((sum, value) => sum + value, 0));
  const summary = {
    model,
    totals,
    totalMean: mean(totals),
    totalSd: populationSd(totals),
    totalRange: Math.max(...totals) - Math.min(...totals),
    tasks: scores.tasks.map((task, index) => {
      const values = runs.map((run) => run[index]);
      return {
        task,
        values,
        mean: mean(values),
        sd: populationSd(values),
        range: Math.max(...values) - Math.min(...values),
      };
    }),
  };
  if (scores.validationContracts !== undefined) {
    summary.validationContracts = scores.validationContracts;
  }
  if (suiteByTask !== undefined) {
    summary.suites = summarizeSuites(scores, runs, suiteByTask);
  }
  return summary;
}

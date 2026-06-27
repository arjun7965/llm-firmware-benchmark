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

export function summarizeModelScores(scores, model) {
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
  return {
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
}

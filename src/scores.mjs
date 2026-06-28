import { readFileSync } from "node:fs";

const taskIdPattern = /^[a-z0-9]+(?:-[a-z0-9]+)*$/;
const modelIdPattern = /^[a-z0-9]+(?:[._-][a-z0-9]+)*$/;
const runNamePattern = /^run[1-9][0-9]*$/;
const reservedKeys = new Set(["rubric", "tasks"]);

function isObject(value) {
  return value && typeof value === "object" && !Array.isArray(value);
}

export function scoreModelIds(scores) {
  return Object.keys(scores).filter((key) => !reservedKeys.has(key));
}

export function validateScores(scores) {
  if (!isObject(scores)) {
    throw new TypeError("scores must be an object");
  }
  if (typeof scores.rubric !== "string" || scores.rubric.trim() === "") {
    throw new TypeError("scores.rubric must be a non-empty string");
  }
  if (!Array.isArray(scores.tasks) || scores.tasks.length === 0 ||
      scores.tasks.some((task) =>
        typeof task !== "string" || !taskIdPattern.test(task))) {
    throw new TypeError("scores.tasks must contain valid task IDs");
  }
  if (new Set(scores.tasks).size !== scores.tasks.length) {
    throw new TypeError("scores.tasks cannot contain duplicates");
  }

  const models = scoreModelIds(scores);
  if (models.length === 0) {
    throw new TypeError("scores must contain at least one model");
  }
  for (const model of models) {
    if (!modelIdPattern.test(model)) {
      throw new TypeError(`invalid score model ID: ${model}`);
    }
    const runs = scores[model];
    if (!isObject(runs) || Object.keys(runs).length === 0) {
      throw new TypeError(`model ${model} must contain runs`);
    }
    for (const [run, values] of Object.entries(runs)) {
      if (!runNamePattern.test(run)) {
        throw new TypeError(`model ${model} has invalid run name: ${run}`);
      }
      if (!Array.isArray(values) || values.length !== scores.tasks.length ||
          values.some((value) =>
            !Number.isFinite(value) || value < 0 || value > 10)) {
        throw new TypeError(`model ${model} has invalid scores for ${run}`);
      }
    }
  }
  return scores;
}

export function loadScores(path) {
  return validateScores(JSON.parse(readFileSync(path, "utf8")));
}

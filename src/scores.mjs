import { readFileSync } from "node:fs";
import {
  validateValidationEnvironmentReference,
  validateValidationProfileReference,
} from "./validation-profiles.mjs";
import { requireScoringMode } from "./scoring-modes.mjs";

const taskIdPattern = /^[a-z0-9]+(?:-[a-z0-9]+)*$/;
const modelIdPattern = /^[a-z0-9]+(?:[._-][a-z0-9]+)*$/;
const runNamePattern = /^run[1-9][0-9]*$/;
const reservedKeys = new Set([
  "rubric",
  "scoringModes",
  "schemaVersion",
  "tasks",
  "validationContracts",
]);

function isObject(value) {
  return value && typeof value === "object" && !Array.isArray(value);
}

export function scoreModelIds(scores) {
  return Object.keys(scores).filter((key) => !reservedKeys.has(key));
}

function validationProfileForTask(validationProfileByTask, task) {
  let validationProfile;
  if (validationProfileByTask instanceof Map) {
    validationProfile = validationProfileByTask.get(task);
  } else if (
    isObject(validationProfileByTask) &&
    Object.hasOwn(validationProfileByTask, task)
  ) {
    validationProfile = validationProfileByTask[task];
  }
  if (typeof validationProfile !== "string") {
    throw new TypeError(
      `missing configured validation profile for scored task: ${task}`,
    );
  }
  return validationProfile;
}

function scoringModeForTask(scoringModeByTask, task) {
  if (scoringModeByTask === undefined) return "deterministic";

  let scoringMode;
  if (scoringModeByTask instanceof Map) {
    scoringMode = scoringModeByTask.get(task);
  } else if (
    isObject(scoringModeByTask) &&
    Object.hasOwn(scoringModeByTask, task)
  ) {
    scoringMode = scoringModeByTask[task];
  }
  return requireScoringMode(
    scoringMode,
    `configured scoringMode for scored task: ${task}`,
  );
}

export function validateScores(scores, {
  validationProfileByTask,
  scoringModeByTask,
} = {}) {
  if (!isObject(scores)) {
    throw new TypeError("scores must be an object");
  }
  if (scores.schemaVersion !== "1.1") {
    throw new TypeError("unsupported scores.schemaVersion");
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
  if (
    !isObject(scores.scoringModes) ||
    Object.keys(scores.scoringModes).sort().join(",") !==
      [...scores.tasks].sort().join(",")
  ) {
    throw new TypeError("scores.scoringModes must cover exactly scores.tasks");
  }
  const deterministicTasks = [];
  for (const task of scores.tasks) {
    const scoringMode = requireScoringMode(
      scores.scoringModes[task],
      `scores.scoringModes.${task}`,
    );
    if (scoringMode !== scoringModeForTask(scoringModeByTask, task)) {
      throw new TypeError(`scores.scoringModes.${task} does not match task`);
    }
    if (scoringMode === "deterministic") deterministicTasks.push(task);
  }
  if (
    !isObject(scores.validationContracts) ||
    Object.keys(scores.validationContracts).sort().join(",") !==
      [...deterministicTasks].sort().join(",")
  ) {
    throw new TypeError(
      "scores.validationContracts must cover exactly deterministic tasks",
    );
  }
  for (const task of deterministicTasks) {
    const contract = scores.validationContracts[task];
    if (
      !isObject(contract) ||
      Object.keys(contract).sort().join(",") !== "environment,profile"
    ) {
      throw new TypeError(
        `scores.validationContracts.${task} has unexpected fields`,
      );
    }
    const profile = validateValidationProfileReference(
      contract.profile,
      `scores.validationContracts.${task}.profile`,
    );
    if (
      profile.id !== validationProfileForTask(
        validationProfileByTask,
        task,
      )
    ) {
      throw new TypeError(
        `scores.validationContracts.${task}.profile does not match task`,
      );
    }
    const environment = validateValidationEnvironmentReference(
      contract.environment,
      `scores.validationContracts.${task}.environment`,
    );
    if (
      !profile.environments?.some((reference) =>
        reference.id === environment.id &&
        reference.revision === environment.revision)
    ) {
      throw new TypeError(
        `scores.validationContracts.${task}.environment is unsupported ` +
        "by its validation profile",
      );
    }
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

export function loadScores(path, options) {
  return validateScores(JSON.parse(readFileSync(path, "utf8")), options);
}

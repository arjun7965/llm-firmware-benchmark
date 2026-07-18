const scoringModeSet = new Set([
  "deterministic",
  "rubric-only",
]);

const rubricOnlyReasonSet = new Set([
  "undocumented-service",
  "environment-dependent-scoring",
]);

export const scoringModeIds = Object.freeze([...scoringModeSet]);
export const rubricOnlyReasonIds = Object.freeze([...rubricOnlyReasonSet]);

export function requireScoringMode(value, name = "scoringMode") {
  if (typeof value !== "string" || !scoringModeSet.has(value)) {
    throw new TypeError(`${name} is invalid`);
  }
  return value;
}

export function validateRubricOnlyMetadata(task, name = "task") {
  const scoringMode = requireScoringMode(task.scoringMode, `${name} scoringMode`);
  const hasReasons = Object.hasOwn(task, "rubricOnlyReasons");
  const hasRationale = Object.hasOwn(task, "rubricOnlyRationale");

  if (scoringMode === "deterministic") {
    if (hasReasons || hasRationale) {
      throw new TypeError(
        `${name} deterministic scoringMode cannot define rubric-only metadata`,
      );
    }
    return task;
  }

  if (
    !hasReasons ||
    !Array.isArray(task.rubricOnlyReasons) ||
    task.rubricOnlyReasons.length === 0 ||
    task.rubricOnlyReasons.some((reason) =>
      typeof reason !== "string" || !rubricOnlyReasonSet.has(reason)) ||
    new Set(task.rubricOnlyReasons).size !== task.rubricOnlyReasons.length
  ) {
    throw new TypeError(`${name} rubricOnlyReasons is invalid`);
  }
  if (
    !hasRationale ||
    typeof task.rubricOnlyRationale !== "string" ||
    task.rubricOnlyRationale.trim() === "" ||
    task.rubricOnlyRationale.includes("\0")
  ) {
    throw new TypeError(`${name} rubricOnlyRationale is invalid`);
  }
  return task;
}

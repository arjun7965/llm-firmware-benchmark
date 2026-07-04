export const suiteIds = Object.freeze([
  "firmware",
  "auxiliary",
]);

export const suiteSet = new Set(suiteIds);

export function requireSuite(value, name = "suite") {
  if (typeof value !== "string" || !suiteSet.has(value)) {
    throw new TypeError(`${name} must be firmware or auxiliary`);
  }
  return value;
}

export function resultSuite(result) {
  if (result?.suite !== undefined) {
    return requireSuite(result.suite, "result suite");
  }
  return result?.targetProfile == null ? "auxiliary" : "firmware";
}

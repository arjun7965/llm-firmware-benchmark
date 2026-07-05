export const validationProfileIds = Object.freeze([
  "c11-host",
  "go-std",
  "node-typescript",
  "node-typescript-postgresql",
  "postgresql",
  "python3-pytest-hypothesis",
  "python3-stdlib",
  "react18-typescript",
  "stable-rust",
]);

export const validationProfileSet = new Set(validationProfileIds);

export function requireValidationProfile(
  value,
  name = "validationProfile",
) {
  if (typeof value !== "string" || !validationProfileSet.has(value)) {
    throw new TypeError(`${name} is invalid`);
  }
  return value;
}

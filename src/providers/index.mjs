import { executeNcodeJob } from "./ncode.mjs";

const providers = new Map([
  ["ncode", executeNcodeJob],
]);

export function getProvider(name) {
  const provider = providers.get(name);
  if (!provider) {
    throw new TypeError(
      `unknown provider "${name}"; add an adapter in src/providers/index.mjs`,
    );
  }
  return provider;
}

export function generateWithProvider(job) {
  return getProvider(job.provider)(job);
}

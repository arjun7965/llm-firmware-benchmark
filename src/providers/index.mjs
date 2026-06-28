import { executeNcodeJob } from "./ncode.mjs";
import { executeOpenAICompatibleJob } from "./openai-compatible.mjs";

const providers = new Map([
  ["ncode", executeNcodeJob],
  ["openai-compatible", executeOpenAICompatibleJob],
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

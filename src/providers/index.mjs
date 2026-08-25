import { executeClaudeCodeJob } from "./claude-code.mjs";
import { executeCodexJob } from "./codex.mjs";
import {
  executeOpenCodeJob,
  openCodeProviderConfigSha256,
} from "./opencode.mjs";
import { executeOpenAICompatibleJob } from "./openai-compatible.mjs";

const providers = new Map([
  ["claude-code", executeClaudeCodeJob],
  ["codex", executeCodexJob],
  ["opencode", executeOpenCodeJob],
  ["openai-compatible", executeOpenAICompatibleJob],
]);
const providerConfigSha256ByName = new Map([
  ["opencode", openCodeProviderConfigSha256],
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

export function getProviderConfigSha256(name) {
  getProvider(name);
  return providerConfigSha256ByName.get(name);
}

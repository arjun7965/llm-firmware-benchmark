import { executeClaudeCodeJob } from "./claude-code.mjs";
import { executeCodexJob } from "./codex.mjs";
import { executeNcodeJob } from "./ncode.mjs";
import { executeOpenCodeJob } from "./opencode.mjs";
import { executeOpenAICompatibleJob } from "./openai-compatible.mjs";

const providers = new Map([
  ["claude-code", executeClaudeCodeJob],
  ["codex", executeCodexJob],
  ["ncode", executeNcodeJob],
  ["opencode", executeOpenCodeJob],
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

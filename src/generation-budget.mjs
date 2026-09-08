// Null means unreported, not unlimited. Historical defaults are not inferred.
export function describeGenerationBudget(provider, options = {}, current = true) {
  const cli = ["codex", "claude-code", "opencode"].includes(provider);
  const defaultTimeout = cli ? 600000 :
    provider === "openai-compatible" ? 300000 : null;
  const effort = options.effort ??
    (current && ["codex", "claude-code"].includes(provider) ? "medium" : null);
  const request = options.request ?? {};
  const tokenLimits = {};
  if (provider === "opencode" && Number.isSafeInteger(options.maxOutputTokens) &&
      options.maxOutputTokens > 0) {
    tokenLimits.max_output_tokens = options.maxOutputTokens;
  }
  for (const name of ["max_tokens", "max_completion_tokens", "max_output_tokens"]) {
    if (Number.isSafeInteger(request[name]) && request[name] > 0) {
      tokenLimits[name] = request[name];
    }
  }
  if (Number.isSafeInteger(request.thinking?.budget_tokens) &&
      request.thinking.budget_tokens > 0) {
    tokenLimits["thinking.budget_tokens"] = request.thinking.budget_tokens;
  }
  const reasoning = options.variant ?? effort ?? request.reasoning_effort ?? null;
  const timeout = options.timeoutMs ?? (current ? defaultTimeout : null);
  return {
    timeoutMs: Number.isSafeInteger(timeout) && timeout > 0 ? timeout : null,
    reasoning: typeof reasoning !== "string" ? null : {
      control: options.variant !== undefined ? "variant" :
        effort !== null ? "effort" : "reasoning_effort",
      value: reasoning,
    },
    configuredTokenLimits: Object.keys(tokenLimits).length ? tokenLimits : null,
    providerTokenLimits: null,
  };
}

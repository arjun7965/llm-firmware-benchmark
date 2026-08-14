# Claude Code Provider

Use provider `claude-code` to run a model through Claude Code's non-interactive
mode. The adapter reuses authentication established by `claude auth login`;
model configuration contains no API key or access token.

```json
{
  "id": "claude-sonnet-5",
  "provider": "claude-code",
  "model": "claude-sonnet-5",
  "options": {
    "effort": "medium",
    "timeoutMs": 600000
  }
}
```

`effort` defaults to `medium`. Supported values are `low`, `medium`, `high`,
`xhigh`, `max`, and `ultracode`; the selected model may support only a subset.
`timeoutMs` defaults to 600000 and must be a positive integer. Prefer a pinned
model identifier over a moving alias when collecting results for comparison.

Each job invokes `claude --print` with the configured model and effort. The
adapter uses an empty temporary working directory, removes it after the process
exits, and applies all of the following controls:

- no session or prompt-history persistence;
- safe mode, with slash commands, Chrome integration, and nonessential network
  traffic disabled;
- no built-in tools, Claude.ai connectors, or MCP tools;
- a non-interactive permission mode and a single-turn limit; and
- JSON output on stdout, which preserves Claude Code usage metadata in the
  private raw result while allowing the harness to extract its `result` text.

These controls keep benchmark tasks self-contained and align Claude Code as
closely as possible with the other CLI providers' no-tools policies. Claude
Code still supplies its own provider-level system prompt, just as other
runtimes may supply their own system instructions. Record the Claude Code CLI
version, authentication mode, model identifier, effort, and any provider-policy
differences when publishing a comparison.

Install Claude Code using an installation method documented by Anthropic, then
authenticate and verify the session without exposing credentials:

```bash
claude auth login
claude auth status
```

Run only the Claude Code configuration from `models.example.json` or an
equivalent local model file:

```bash
npm run benchmark -- --models claude-sonnet-5
```

Raw stdout and stderr remain private under `results/`. Use the repository's
reviewed public-export workflow before publishing any result.

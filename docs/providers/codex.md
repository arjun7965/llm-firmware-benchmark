# Codex Provider

Use provider `codex` to run a model through the Codex CLI's non-interactive
mode. The adapter reuses authentication established by `codex login`; model
configuration contains no API key or access token.

```json
{
  "id": "gpt-5.6-luna",
  "provider": "codex",
  "model": "gpt-5.6-luna",
  "options": {
    "effort": "medium",
    "timeoutMs": 600000
  }
}
```

`effort` defaults to `medium`. Supported values are `minimal`, `low`,
`medium`, `high`, `xhigh`, `max`, and `ultra`; the selected model may support
only a subset. `timeoutMs` defaults to 600000 and must be a positive integer.

Each job invokes `codex exec` with the configured model and effort. The adapter
uses an empty temporary working directory, removes it after the process exits,
and applies all of the following controls:

- ephemeral session storage;
- ignored user configuration and execution-policy rules;
- a read-only sandbox and no Git repository requirement;
- disabled shell, web search, apps, plugins, browser/computer use, image
  generation, skill discovery/dependency installation, goals, and multi-agent
  features; and
- final-answer text on stdout, with Codex progress retained separately on
  stderr in the private raw result.

These controls keep benchmark tasks self-contained and give Codex a fixed
no-tools policy. Codex still supplies its own provider-level agent
instructions, just as other runtimes may supply their own system prompts.
Record the Codex CLI version, authentication mode, model identifier, effort,
and any provider-policy differences when publishing a comparison.

Check authentication without exposing credentials:

```bash
codex login status
```

Run only the three GPT-5.6 Codex configurations from `models.example.json` or
an equivalent local model file:

```bash
npm run benchmark -- \
  --models gpt-5.6-luna,gpt-5.6-sol,gpt-5.6-terra
```

Raw stdout and stderr remain private under `results/`. Use the repository's
reviewed public-export workflow before publishing any result.

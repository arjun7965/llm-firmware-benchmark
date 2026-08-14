# OpenCode Provider

Use provider `opencode` to run a model through OpenCode's non-interactive
mode. The adapter reuses credentials established by `opencode auth login` or
provider environment variables; model configuration contains no API key or
access token.

OpenCode model identifiers include the provider prefix:

```json
{
  "id": "opencode-example",
  "provider": "opencode",
  "model": "provider/model-id",
  "options": {
    "variant": "high",
    "timeoutMs": 600000
  }
}
```

`variant` is optional and is passed to OpenCode unchanged. Variants are
provider- and model-specific; examples include reasoning levels such as
`high` and `max`. `timeoutMs` defaults to 600000 and must be a positive
integer. Prefer pinned model identifiers and variants over moving aliases when
collecting results for comparison.

Each job invokes `opencode --pure run --format json` with the configured model.
The adapter uses an empty temporary working directory, removes it after the
process exits, and applies all of the following controls:

- external plugins and project configuration are disabled;
- global OpenCode configuration, injected config paths, and permission
  overrides are ignored;
- a fixed primary benchmark agent denies every tool permission and subagent
  use;
- automatic updates, sharing, snapshots, and LSP downloads are disabled; and
- raw NDJSON events remain in the private result while answer extraction joins
  the completed `text` event parts.

OpenCode's credential data remains available so saved provider authentication
works. Unlike Codex and Claude Code, OpenCode does not currently expose a
no-session-persistence flag for `run`; it can retain a local session record in
its application data. The temporary project directory is still removed, and
the repository's result files remain private. Record the OpenCode CLI version,
authentication mode, model identifier, variant, session-storage difference,
and any provider-policy differences when publishing a comparison.

The isolated configuration intentionally excludes custom providers or model
definitions from the user's global `opencode.json`. Use a built-in OpenCode
provider with saved authentication or provider environment variables. This
keeps user prompts, agents, tools, MCP servers, and plugins from changing the
benchmark task.

Install OpenCode using an installation method documented by the project, then
authenticate and inspect the configured credentials without exposing their
values:

```bash
opencode auth login
opencode auth list
opencode --version
```

After replacing the placeholder model identifier in `models.local.json`, run
only that configuration with:

```bash
npm run benchmark -- --models opencode-example
```

Raw stdout and stderr remain private under `results/`. Use the repository's
reviewed public-export workflow before publishing any result.

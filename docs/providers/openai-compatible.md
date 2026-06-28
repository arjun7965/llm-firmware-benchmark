# OpenAI-Compatible Provider

Use provider `openai-compatible` with services that implement the non-streaming
`POST /v1/chat/completions` contract. This includes many local model servers and
hosted gateways.

```json
{
  "id": "example-model",
  "provider": "openai-compatible",
  "model": "served-model-id",
  "options": {
    "baseUrl": "http://127.0.0.1:8000/v1",
    "timeoutMs": 300000,
    "systemPrompt": "Return a direct technical answer.",
    "request": {
      "temperature": 0,
      "max_tokens": 4096
    }
  }
}
```

`baseUrl` is required. The adapter appends `/chat/completions`, submits the
benchmark prompt as one user message, requests one non-streaming completion,
and stores only `choices[0].message.content` as provider output.

For an authenticated endpoint, name the environment variable containing the
credential:

```json
{
  "baseUrl": "https://api.example.test/v1",
  "apiKeyEnv": "EXAMPLE_API_KEY"
}
```

The credential is resolved at runtime and sent as a Bearer token. Never place
the credential itself in model configuration. Authenticated non-loopback
endpoints must use HTTPS.

Additional Chat Completions fields belong under `request`. The adapter reserves
`model`, `messages`, `n`, and `stream` to preserve benchmark behavior. Record
all request parameters when publishing a comparison because sampling settings
can affect model parity.

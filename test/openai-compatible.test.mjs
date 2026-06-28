import { createServer } from "node:http";
import { once } from "node:events";
import test from "node:test";
import assert from "node:assert/strict";
import {
  buildOpenAICompatibleRequest,
  executeOpenAICompatibleJob,
} from "../src/providers/openai-compatible.mjs";
import { getProvider } from "../src/providers/index.mjs";

const job = {
  run: 1,
  task: {
    id: "task-one",
    category: "test",
    prompt: "Write a robust parser.",
  },
  provider: "openai-compatible",
  modelName: "local-model",
  modelId: "served-model",
  modelOptions: {},
};

async function startServer(t, handler) {
  const server = createServer(handler);
  server.listen(0, "127.0.0.1");
  await once(server, "listening");
  t.after(async () => {
    server.closeAllConnections();
    await new Promise((resolve) => server.close(resolve));
  });
  const { port } = server.address();
  return `http://127.0.0.1:${port}/v1`;
}

async function readRequest(request) {
  request.setEncoding("utf8");
  let body = "";
  for await (const chunk of request) body += chunk;
  return JSON.parse(body);
}

test("adapter sends a Chat Completions request and extracts the answer", async (t) => {
  let received;
  const credential = ["local", "test", "credential"].join("-");
  const baseUrl = await startServer(t, async (request, response) => {
    received = {
      method: request.method,
      url: request.url,
      authorization: request.headers.authorization,
      body: await readRequest(request),
    };
    response.writeHead(200, { "content-type": "application/json" });
    response.end(JSON.stringify({
      choices: [{
        message: {
          role: "assistant",
          content: "Use a state machine.",
        },
      }],
    }));
  });
  const result = await executeOpenAICompatibleJob({
    ...job,
    modelOptions: {
      baseUrl,
      apiKeyEnv: "BENCHMARK_TEST_KEY",
      systemPrompt: "Return only the proposed solution.",
      request: {
        temperature: 0,
        max_tokens: 512,
      },
    },
  }, {
    env: { BENCHMARK_TEST_KEY: credential },
  });

  assert.deepEqual(result, {
    exitCode: 0,
    signal: null,
    stdout: "Use a state machine.",
    stderr: "",
    error: null,
  });
  assert.equal(received.method, "POST");
  assert.equal(received.url, "/v1/chat/completions");
  assert.equal(received.authorization, `Bearer ${credential}`);
  assert.deepEqual(received.body, {
    temperature: 0,
    max_tokens: 512,
    model: "served-model",
    messages: [
      {
        role: "system",
        content: "Return only the proposed solution.",
      },
      {
        role: "user",
        content: "Write a robust parser.",
      },
    ],
    n: 1,
    stream: false,
  });
});

test("adapter supports unauthenticated local endpoints", () => {
  const request = buildOpenAICompatibleRequest({
    ...job,
    modelOptions: {
      baseUrl: "http://localhost:8080/v1/",
    },
  });

  assert.equal(request.url.href, "http://localhost:8080/v1/chat/completions");
  assert.equal("authorization" in request.headers, false);
});

test("adapter rejects unsafe or ambiguous configuration", () => {
  assert.throws(
    () => buildOpenAICompatibleRequest({
      ...job,
      modelOptions: {
        baseUrl: "http://example.test/v1",
        apiKeyEnv: "BENCHMARK_TEST_KEY",
      },
    }, {
      env: { BENCHMARK_TEST_KEY: "present" },
    }),
    /require HTTPS/,
  );
  assert.throws(
    () => buildOpenAICompatibleRequest({
      ...job,
      modelOptions: {
        baseUrl: "http://localhost:8080/v1",
        request: { model: "override" },
      },
    }),
    /cannot override model/,
  );
  const credentialField = ["api", "key"].join("_");
  assert.throws(
    () => buildOpenAICompatibleRequest({
      ...job,
      modelOptions: {
        baseUrl: "http://localhost:8080/v1",
        request: { metadata: { [credentialField]: "stored-value" } },
      },
    }),
    /looks like a credential/,
  );
});

test("adapter reports HTTP and malformed-response failures", async (t) => {
  let responseNumber = 0;
  const baseUrl = await startServer(t, (_request, response) => {
    responseNumber++;
    if (responseNumber === 1) {
      response.writeHead(503, { "content-type": "application/json" });
      response.end(JSON.stringify({ error: "temporarily unavailable" }));
      return;
    }
    response.writeHead(200, { "content-type": "application/json" });
    response.end("not JSON");
  });
  const configuredJob = {
    ...job,
    modelOptions: { baseUrl },
  };

  const unavailable = await executeOpenAICompatibleJob(configuredJob);
  const malformed = await executeOpenAICompatibleJob(configuredJob);

  assert.equal(unavailable.exitCode, 1);
  assert.match(unavailable.error, /HTTP 503/);
  assert.match(unavailable.stderr, /temporarily unavailable/);
  assert.equal(malformed.exitCode, 1);
  assert.match(malformed.error, /invalid JSON/);
});

test("adapter enforces its request timeout", async (t) => {
  const baseUrl = await startServer(t, () => {});

  const result = await executeOpenAICompatibleJob({
    ...job,
    modelOptions: {
      baseUrl,
      timeoutMs: 20,
    },
  });

  assert.equal(result.exitCode, 1);
  assert.match(result.error, /timed out after 20 ms/);
});

test("provider registry exposes the OpenAI-compatible adapter", () => {
  assert.equal(getProvider("openai-compatible"), executeOpenAICompatibleJob);
});

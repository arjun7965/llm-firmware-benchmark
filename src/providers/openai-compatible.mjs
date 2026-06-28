const defaultTimeoutMs = 300_000;
const maximumErrorBodyLength = 4_096;
const environmentNamePattern = /^[A-Za-z_][A-Za-z0-9_]*$/;
const reservedRequestFields = new Set([
  "messages",
  "model",
  "n",
  "stream",
]);
const credentialFieldPattern =
  /^(?:api[_-]?key|access[_-]?token|auth[_-]?token|password|secret)$/i;

function failure(error, stderr = "") {
  return {
    exitCode: 1,
    signal: null,
    stdout: "",
    stderr,
    error,
  };
}

function isPlainObject(value) {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    return false;
  }
  const prototype = Object.getPrototypeOf(value);
  return prototype === Object.prototype || prototype === null;
}

function validateRequestValue(value, path = "request") {
  if (value === null || typeof value === "string" ||
      typeof value === "boolean" || Number.isFinite(value)) {
    return;
  }
  if (Array.isArray(value)) {
    value.forEach((item, index) =>
      validateRequestValue(item, `${path}[${index}]`));
    return;
  }
  if (!isPlainObject(value)) {
    throw new TypeError(`${path} must contain only JSON-compatible values`);
  }
  for (const [key, child] of Object.entries(value)) {
    if (credentialFieldPattern.test(key)) {
      throw new TypeError(
        `${path}.${key} looks like a credential; use apiKeyEnv instead`,
      );
    }
    validateRequestValue(child, `${path}.${key}`);
  }
}

function isLoopback(hostname) {
  return [
    "127.0.0.1",
    "::1",
    "[::1]",
    "localhost",
  ].includes(hostname.toLowerCase());
}

function parseBaseUrl(value) {
  if (typeof value !== "string" || value.trim() === "") {
    throw new TypeError(
      "OpenAI-compatible baseUrl must be a non-empty URL",
    );
  }

  let url;
  try {
    url = new URL(value);
  } catch {
    throw new TypeError("OpenAI-compatible baseUrl must be a valid URL");
  }
  if (!["http:", "https:"].includes(url.protocol)) {
    throw new TypeError("OpenAI-compatible baseUrl must use HTTP or HTTPS");
  }
  if (url.username || url.password || url.search || url.hash) {
    throw new TypeError(
      "OpenAI-compatible baseUrl cannot contain credentials, query, or hash",
    );
  }
  return url;
}

export function buildOpenAICompatibleRequest(job, {
  env = process.env,
} = {}) {
  const options = job.modelOptions ?? {};
  if (!isPlainObject(options)) {
    throw new TypeError("OpenAI-compatible options must be an object");
  }
  const supportedOptions = new Set([
    "apiKeyEnv",
    "baseUrl",
    "request",
    "systemPrompt",
    "timeoutMs",
  ]);
  const unknownOption = Object.keys(options)
    .find((key) => !supportedOptions.has(key));
  if (unknownOption) {
    throw new TypeError(
      `unsupported OpenAI-compatible option: ${unknownOption}`,
    );
  }

  const baseUrl = parseBaseUrl(options.baseUrl);
  const timeoutMs = options.timeoutMs ?? defaultTimeoutMs;
  if (!Number.isInteger(timeoutMs) || timeoutMs < 1) {
    throw new TypeError(
      "OpenAI-compatible timeoutMs must be a positive integer",
    );
  }
  if (typeof job.modelId !== "string" || job.modelId.trim() === "") {
    throw new TypeError(
      "OpenAI-compatible model identifier must be a non-empty string",
    );
  }
  if (typeof job.task?.prompt !== "string" ||
      job.task.prompt.trim() === "") {
    throw new TypeError(
      "OpenAI-compatible task prompt must be a non-empty string",
    );
  }

  const requestOptions = options.request ?? {};
  if (!isPlainObject(requestOptions)) {
    throw new TypeError("OpenAI-compatible request must be an object");
  }
  const reservedField = Object.keys(requestOptions)
    .find((key) => reservedRequestFields.has(key));
  if (reservedField) {
    throw new TypeError(
      `OpenAI-compatible request cannot override ${reservedField}`,
    );
  }
  validateRequestValue(requestOptions);

  const messages = [];
  if (options.systemPrompt !== undefined) {
    if (typeof options.systemPrompt !== "string" ||
        options.systemPrompt.trim() === "") {
      throw new TypeError(
        "OpenAI-compatible systemPrompt must be a non-empty string",
      );
    }
    messages.push({ role: "system", content: options.systemPrompt });
  }
  messages.push({ role: "user", content: job.task.prompt });

  const headers = {
    accept: "application/json",
    "content-type": "application/json",
  };
  if (options.apiKeyEnv !== undefined) {
    if (typeof options.apiKeyEnv !== "string" ||
        !environmentNamePattern.test(options.apiKeyEnv)) {
      throw new TypeError(
        "OpenAI-compatible apiKeyEnv must be an environment variable name",
      );
    }
    const credentialValue = env[options.apiKeyEnv];
    if (typeof credentialValue !== "string" ||
        credentialValue.trim() === "") {
      throw new TypeError(
        `OpenAI-compatible API key environment variable ` +
        `${options.apiKeyEnv} is not set`,
      );
    }
    if (baseUrl.protocol !== "https:" && !isLoopback(baseUrl.hostname)) {
      throw new TypeError(
        "OpenAI-compatible API keys require HTTPS except on loopback hosts",
      );
    }
    headers.authorization = `Bearer ${credentialValue}`;
  }

  const baseHref = baseUrl.href.endsWith("/")
    ? baseUrl.href
    : `${baseUrl.href}/`;
  return {
    url: new URL("chat/completions", baseHref),
    timeoutMs,
    headers,
    body: {
      ...requestOptions,
      model: job.modelId,
      messages,
      n: 1,
      stream: false,
    },
  };
}

export async function executeOpenAICompatibleJob(job, {
  env = process.env,
  fetchImpl = globalThis.fetch,
} = {}) {
  let request;
  try {
    request = buildOpenAICompatibleRequest(job, { env });
  } catch (error) {
    return failure(error.message);
  }
  if (typeof fetchImpl !== "function") {
    return failure("OpenAI-compatible fetch implementation is unavailable");
  }

  const controller = new AbortController();
  let timedOut = false;
  const timer = setTimeout(() => {
    timedOut = true;
    controller.abort();
  }, request.timeoutMs);

  let response;
  let responseText;
  try {
    response = await fetchImpl(request.url, {
      method: "POST",
      headers: request.headers,
      body: JSON.stringify(request.body),
      signal: controller.signal,
    });
    responseText = await response.text();
  } catch (error) {
    if (timedOut) {
      return failure(
        `OpenAI-compatible request timed out after ${request.timeoutMs} ms`,
      );
    }
    return failure(`OpenAI-compatible request failed: ${error.message}`);
  } finally {
    clearTimeout(timer);
  }

  const errorBody = responseText.slice(0, maximumErrorBodyLength);
  if (!response.ok) {
    return failure(
      `OpenAI-compatible API returned HTTP ${response.status}`,
      errorBody,
    );
  }

  let document;
  try {
    document = JSON.parse(responseText);
  } catch {
    return failure(
      "OpenAI-compatible API returned invalid JSON",
      errorBody,
    );
  }
  const answer = document?.choices?.[0]?.message?.content;
  if (typeof answer !== "string") {
    return failure(
      "OpenAI-compatible API response is missing choices[0].message.content",
    );
  }

  return {
    exitCode: 0,
    signal: null,
    stdout: answer,
    stderr: "",
    error: null,
  };
}

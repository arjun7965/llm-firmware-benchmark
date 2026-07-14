import assert from "node:assert/strict";
import { createHmac, randomUUID } from "node:crypto";
import { readFileSync, rmSync } from "node:fs";
import {
  createServer,
  request as sendRequest,
  type Server,
} from "node:http";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { Pool } from "pg";
import { createWebhookApp } from "../../generated/handler";

const NOW_MS = 1_700_000_000_000;
const NOW_TIMESTAMP = String(NOW_MS / 1000);
const CURRENT_KEY = "current-webhook-key";
const PREVIOUS_KEY = "previous-webhook-key";

interface HttpReply {
  json: unknown;
  status: number;
  text: string;
}

interface WebhookRequest {
  body: string;
  signingKey?: string;
  signature?: string;
  timestamp?: string;
  webhookId?: string;
}

function asObject(value: unknown): Record<string, unknown> {
  assert.ok(value && typeof value === "object" && !Array.isArray(value));
  return value as Record<string, unknown>;
}

function errorCode(reply: HttpReply): string {
  return String(asObject(reply.json).error);
}

function sign(signingKey: string, timestamp: string, body: string): string {
  return createHmac("sha256", signingKey)
    .update(`${timestamp}.`, "utf8")
    .update(Buffer.from(body, "utf8"))
    .digest("hex");
}

function postWebhook(
  socketPath: string,
  { body, signingKey, signature, timestamp, webhookId }: WebhookRequest,
): Promise<HttpReply> {
  return new Promise((resolve, reject) => {
    const headers: Record<string, string> = {
      "content-length": String(Buffer.byteLength(body)),
      "content-type": "application/json",
    };
    if (webhookId !== undefined) headers["x-webhook-id"] = webhookId;
    if (timestamp !== undefined) headers["x-webhook-timestamp"] = timestamp;
    if (signature !== undefined) {
      headers["x-webhook-signature"] = signature;
    } else if (signingKey !== undefined && timestamp !== undefined) {
      headers["x-webhook-signature"] = sign(signingKey, timestamp, body);
    }
    const request = sendRequest({
      socketPath,
      method: "POST",
      path: "/webhook",
      headers,
    }, (response) => {
      const chunks: Buffer[] = [];
      response.on("data", (chunk: Buffer) => chunks.push(Buffer.from(chunk)));
      response.once("error", reject);
      response.once("end", () => {
        const text = Buffer.concat(chunks).toString("utf8");
        let json: unknown = null;
        try {
          json = JSON.parse(text);
        } catch {
          // The status assertion exposes an unexpected non-JSON response.
        }
        resolve({ status: response.statusCode ?? 0, text, json });
      });
    });
    request.once("error", reject);
    request.end(body);
  });
}

function listen(server: Server, socketPath: string): Promise<void> {
  return new Promise((resolve, reject) => {
    server.once("error", reject);
    server.listen(socketPath, () => {
      server.off("error", reject);
      resolve();
    });
  });
}

function close(server: Server): Promise<void> {
  return new Promise((resolve, reject) => {
    server.close((error) => error ? reject(error) : resolve());
  });
}

function assertAccepted(reply: HttpReply): void {
  assert.equal(reply.status, 202, reply.text);
  assert.deepEqual(reply.json, { status: "accepted" });
}

async function count(
  pool: Pool,
  query: string,
  values: unknown[] = [],
): Promise<number> {
  const result = await pool.query<{ count: number }>(query, values);
  return result.rows[0]?.count ?? -1;
}

async function main(): Promise<void> {
  const schemaPath = process.argv[2];
  const handlerPath = process.argv[3];
  if (!schemaPath || !handlerPath) {
    throw new Error("expected schema and handler paths");
  }
  const handlerSource = readFileSync(handlerPath, "utf8");
  assert.match(handlerSource, /\btimingSafeEqual\s*\(/u);

  const primaryPool = new Pool({ max: 8 });
  const secondaryPool = new Pool({ max: 8 });
  const firstSocket = join(tmpdir(), `webhook-${randomUUID()}.sock`);
  const secondSocket = join(tmpdir(), `webhook-${randomUUID()}.sock`);
  const firstServer = createServer(createWebhookApp(
    primaryPool,
    [CURRENT_KEY, PREVIOUS_KEY],
    () => NOW_MS,
  ));
  const secondServer = createServer(createWebhookApp(
    secondaryPool,
    [CURRENT_KEY, PREVIOUS_KEY],
    () => NOW_MS,
  ));

  try {
    await primaryPool.query(readFileSync(schemaPath, "utf8"));
    const role = await primaryPool.query<{
      current_user: string;
      validator_can_login: boolean;
    }>(
      `SELECT current_user,
              (SELECT rolcanlogin FROM pg_roles WHERE rolname = 'validator')
                AS validator_can_login`,
    );
    assert.deepEqual(role.rows[0], {
      current_user: "benchmark",
      validator_can_login: false,
    });
    await primaryPool.query(
      `INSERT INTO public.webhook_events (
         webhook_id, authenticated_sha256, signed_timestamp, payload
       ) VALUES ($1, $2, $3, $4::jsonb)`,
      ["outbox-constraint-probe", "a".repeat(64), 1, "{}"],
    );
    await assert.rejects(primaryPool.query(
      `INSERT INTO public.webhook_outbox (webhook_id, topic, payload)
       VALUES ($1, 'not-a-webhook-topic', $2::jsonb)`,
      ["outbox-constraint-probe", "{}"],
    ));
    await primaryPool.query(
      `INSERT INTO public.webhook_outbox (webhook_id, topic, payload)
       VALUES ($1, 'webhook.received', $2::jsonb)`,
      ["outbox-constraint-probe", "{}"],
    );
    await assert.rejects(primaryPool.query(
      `INSERT INTO public.webhook_outbox (webhook_id, topic, payload)
       VALUES ($1, 'webhook.received', $2::jsonb)`,
      ["outbox-constraint-probe", "{}"],
    ));
    await listen(firstServer, firstSocket);
    await listen(secondServer, secondSocket);

    const firstBody = '{"type":"invoice.created","amount":10}';
    const first = await postWebhook(firstSocket, {
      body: firstBody,
      signingKey: CURRENT_KEY,
      timestamp: NOW_TIMESTAMP,
      webhookId: "first-delivery",
    });
    assertAccepted(first);
    const replay = await postWebhook(secondSocket, {
      body: firstBody,
      signingKey: CURRENT_KEY,
      timestamp: NOW_TIMESTAMP,
      webhookId: "first-delivery",
    });
    assert.deepEqual(replay, first);
    assert.equal(await count(
      primaryPool,
      "SELECT count(*)::integer AS count FROM public.webhook_events WHERE webhook_id = 'first-delivery'",
    ), 1);
    assert.equal(await count(
      primaryPool,
      "SELECT count(*)::integer AS count FROM public.webhook_outbox WHERE webhook_id = 'first-delivery'",
    ), 1);

    const concurrentBody = '{"type":"subscription.updated","plan":"pro"}';
    const concurrent = await Promise.all(
      Array.from({ length: 8 }, (_value, index) => postWebhook(
        index % 2 === 0 ? firstSocket : secondSocket,
        {
          body: concurrentBody,
          signingKey: CURRENT_KEY,
          timestamp: NOW_TIMESTAMP,
          webhookId: "concurrent-delivery",
        },
      )),
    );
    for (const reply of concurrent) assertAccepted(reply);
    assert.equal(await count(
      primaryPool,
      "SELECT count(*)::integer AS count FROM public.webhook_events WHERE webhook_id = 'concurrent-delivery'",
    ), 1);
    assert.equal(await count(
      primaryPool,
      "SELECT count(*)::integer AS count FROM public.webhook_outbox WHERE webhook_id = 'concurrent-delivery'",
    ), 1);

    const reused = await postWebhook(firstSocket, {
      body: '{"type":"invoice.updated","amount":10}',
      signingKey: CURRENT_KEY,
      timestamp: NOW_TIMESTAMP,
      webhookId: "first-delivery",
    });
    assert.equal(reused.status, 409);
    assert.equal(errorCode(reused), "webhook_id_reused");

    const rawBody = '{"type":"raw.binding","amount":4}';
    assertAccepted(await postWebhook(firstSocket, {
      body: rawBody,
      signingKey: CURRENT_KEY,
      timestamp: NOW_TIMESTAMP,
      webhookId: "raw-body",
    }));
    const whitespaceChanged = await postWebhook(secondSocket, {
      body: '{"type":"raw.binding", "amount":4}',
      signingKey: CURRENT_KEY,
      timestamp: NOW_TIMESTAMP,
      webhookId: "raw-body",
    });
    assert.equal(whitespaceChanged.status, 409);
    assert.equal(errorCode(whitespaceChanged), "webhook_id_reused");

    assertAccepted(await postWebhook(firstSocket, {
      body: '{"type":"legacy.secret"}',
      signingKey: PREVIOUS_KEY,
      timestamp: NOW_TIMESTAMP,
      webhookId: "rotated-secret",
    }));

    const unauthenticatedMalformed = await postWebhook(firstSocket, {
      body: "not-json",
      signature: "00".repeat(32),
      timestamp: NOW_TIMESTAMP,
      webhookId: "unauthenticated-malformed",
    });
    assert.equal(unauthenticatedMalformed.status, 401);
    assert.equal(errorCode(unauthenticatedMalformed), "invalid_signature");
    const authenticatedMalformed = await postWebhook(firstSocket, {
      body: "not-json",
      signingKey: CURRENT_KEY,
      timestamp: NOW_TIMESTAMP,
      webhookId: "authenticated-malformed",
    });
    assert.equal(authenticatedMalformed.status, 400);
    assert.equal(errorCode(authenticatedMalformed), "invalid_body");

    const staleTimestamp = String(NOW_MS / 1000 - 10 * 60);
    const stale = await postWebhook(firstSocket, {
      body: '{"type":"stale"}',
      signingKey: CURRENT_KEY,
      timestamp: staleTimestamp,
      webhookId: "stale-delivery",
    });
    assert.equal(stale.status, 400);
    assert.equal(errorCode(stale), "timestamp_out_of_range");
    const missingId = await postWebhook(firstSocket, {
      body: '{"type":"missing.id"}',
      signingKey: CURRENT_KEY,
      timestamp: NOW_TIMESTAMP,
    });
    assert.equal(missingId.status, 400);
    assert.equal(errorCode(missingId), "invalid_request");

    await primaryPool.query(
      `CREATE FUNCTION public.reject_atomic_outbox()
       RETURNS trigger
       LANGUAGE plpgsql
       AS $function$
       BEGIN
         IF NEW.webhook_id = 'atomic-failure' THEN
           RAISE EXCEPTION 'forced outbox insert failure';
         END IF;
         RETURN NEW;
       END;
       $function$`,
    );
    await primaryPool.query(
      `CREATE TRIGGER reject_atomic_outbox
       BEFORE INSERT ON public.webhook_outbox
       FOR EACH ROW EXECUTE FUNCTION public.reject_atomic_outbox()`,
    );
    const atomicFailure = await postWebhook(firstSocket, {
      body: '{"type":"atomic.failure"}',
      signingKey: CURRENT_KEY,
      timestamp: NOW_TIMESTAMP,
      webhookId: "atomic-failure",
    });
    assert.equal(atomicFailure.status, 500, atomicFailure.text);
    assert.equal(await count(
      primaryPool,
      "SELECT count(*)::integer AS count FROM public.webhook_events WHERE webhook_id = 'atomic-failure'",
    ), 0);
    assert.equal(await count(
      primaryPool,
      "SELECT count(*)::integer AS count FROM public.webhook_outbox WHERE webhook_id = 'atomic-failure'",
    ), 0);
    await primaryPool.query("DROP TRIGGER reject_atomic_outbox ON public.webhook_outbox");
    await primaryPool.query("DROP FUNCTION public.reject_atomic_outbox()");
  } finally {
    if (firstServer.listening) await close(firstServer);
    if (secondServer.listening) await close(secondServer);
    rmSync(firstSocket, { force: true });
    rmSync(secondSocket, { force: true });
    await primaryPool.end();
    await secondaryPool.end();
  }
}

main().then(
  () => console.log("webhook-replay-security tests passed"),
  (error: unknown) => {
    console.error(error);
    process.exitCode = 1;
  },
);

import {
  createHash,
  createHmac,
  timingSafeEqual,
} from "node:crypto";
import express, {
  type Express,
  type NextFunction,
  type Request,
  type Response,
} from "express";
import type { Pool, PoolClient } from "pg";
import type { WebhookSuccess } from "../starter/webhook_api";

const MAX_WEBHOOK_BYTES = 16 * 1024;
const MAX_TIMESTAMP_SKEW_MS = 5 * 60 * 1000;

interface ExistingWebhook {
  authenticated_sha256: string;
}

interface ParsedTimestamp {
  milliseconds: number;
  seconds: number;
}

const acceptedResponse: WebhookSuccess = { status: "accepted" };

function isValidWebhookId(value: string | undefined): value is string {
  return (
    typeof value === "string" &&
    value.length >= 1 &&
    value.length <= 128 &&
    value.trim() === value
  );
}

function isValidSignature(value: string | undefined): value is string {
  return typeof value === "string" && /^[0-9a-f]{64}$/u.test(value);
}

function parseTimestamp(value: string | undefined): ParsedTimestamp | null {
  if (typeof value !== "string" || !/^[0-9]+$/u.test(value)) return null;
  const seconds = Number(value);
  if (!Number.isSafeInteger(seconds)) return null;
  const milliseconds = seconds * 1000;
  if (!Number.isSafeInteger(milliseconds)) return null;
  return { milliseconds, seconds };
}

function isJsonObject(value: unknown): value is Record<string, unknown> {
  return Boolean(value) && typeof value === "object" && !Array.isArray(value);
}

function hasValidSignature(
  secrets: readonly string[],
  timestamp: string,
  rawBody: Buffer,
  signature: string,
): boolean {
  if (!isValidSignature(signature)) return false;
  const provided = Buffer.from(signature, "hex");
  if (provided.length !== 32) return false;
  let matches = false;
  const signedPrefix = Buffer.from(`${timestamp}.`, "utf8");
  for (const secret of secrets) {
    const expected = createHmac("sha256", secret)
      .update(signedPrefix)
      .update(rawBody)
      .digest();
    matches = timingSafeEqual(expected, provided) || matches;
  }
  return matches;
}

function contentSha256(timestamp: string, rawBody: Buffer): string {
  return createHash("sha256")
    .update(timestamp, "utf8")
    .update(".", "utf8")
    .update(rawBody)
    .digest("hex");
}

function accepted(response: Response): Response {
  return response.status(202).json(acceptedResponse);
}

async function rollback(client: PoolClient): Promise<void> {
  try {
    await client.query("ROLLBACK");
  } catch {
    // Preserve the original failure as the response cause.
  }
}

export function createWebhookApp(
  pool: Pool,
  secrets: readonly string[],
  now: () => number = Date.now,
): Express {
  if (
    !Array.isArray(secrets) ||
    secrets.length === 0 ||
    secrets.some((secret) => typeof secret !== "string" || secret.length === 0)
  ) {
    throw new TypeError("secrets must contain at least one nonempty value");
  }
  const signingSecrets = [...secrets];
  const app = express();
  app.use(express.raw({ type: "application/json", limit: MAX_WEBHOOK_BYTES }));

  app.post("/webhook", async (request, response) => {
    const webhookId = request.get("x-webhook-id");
    const timestamp = request.get("x-webhook-timestamp");
    const signature = request.get("x-webhook-signature");
    const parsedTimestamp = parseTimestamp(timestamp);
    const rawBody = request.body;
    if (
      !isValidWebhookId(webhookId) ||
      typeof timestamp !== "string" ||
      !parsedTimestamp ||
      !isValidSignature(signature)
    ) {
      return response.status(400).json({ error: "invalid_request" });
    }
    if (!Buffer.isBuffer(rawBody)) {
      return response.status(400).json({ error: "invalid_body" });
    }
    if (!hasValidSignature(signingSecrets, timestamp, rawBody, signature)) {
      return response.status(401).json({ error: "invalid_signature" });
    }
    if (Math.abs(now() - parsedTimestamp.milliseconds) > MAX_TIMESTAMP_SKEW_MS) {
      return response.status(400).json({ error: "timestamp_out_of_range" });
    }

    let payload: Record<string, unknown>;
    try {
      const parsed = JSON.parse(rawBody.toString("utf8"));
      if (!isJsonObject(parsed)) {
        return response.status(400).json({ error: "invalid_body" });
      }
      payload = parsed;
    } catch {
      return response.status(400).json({ error: "invalid_body" });
    }
    const serializedPayload = JSON.stringify(payload);
    if (serializedPayload === undefined) {
      return response.status(400).json({ error: "invalid_body" });
    }
    const authenticatedSha256 = contentSha256(timestamp, rawBody);
    let client: PoolClient | null = null;
    let transactionOpen = false;

    try {
      client = await pool.connect();
      await client.query("BEGIN");
      transactionOpen = true;
      const claimed = await client.query(
        `INSERT INTO public.webhook_events (
           webhook_id, authenticated_sha256, signed_timestamp, payload
         ) VALUES ($1, $2, $3, $4::jsonb)
         ON CONFLICT (webhook_id) DO NOTHING
         RETURNING webhook_id`,
        [
          webhookId,
          authenticatedSha256,
          parsedTimestamp.seconds,
          serializedPayload,
        ],
      );

      if (claimed.rowCount === 0) {
        const existing = await client.query<ExistingWebhook>(
          `SELECT authenticated_sha256
             FROM public.webhook_events
            WHERE webhook_id = $1
            FOR UPDATE`,
          [webhookId],
        );
        if (existing.rowCount !== 1) {
          throw new Error("webhook event disappeared");
        }
        if (existing.rows[0]?.authenticated_sha256 !== authenticatedSha256) {
          await rollback(client);
          transactionOpen = false;
          return response.status(409).json({ error: "webhook_id_reused" });
        }
        await client.query("COMMIT");
        transactionOpen = false;
        return accepted(response);
      }

      const outbox = await client.query(
        `INSERT INTO public.webhook_outbox (webhook_id, topic, payload)
         VALUES ($1, 'webhook.received', $2::jsonb)
         RETURNING id`,
        [webhookId, serializedPayload],
      );
      if (outbox.rowCount !== 1) {
        throw new Error("outbox insert did not return one row");
      }
      await client.query("COMMIT");
      transactionOpen = false;
      return accepted(response);
    } catch {
      if (client && transactionOpen) await rollback(client);
      return response.status(500).json({ error: "internal_error" });
    } finally {
      client?.release();
    }
  });

  app.use((
    _error: unknown,
    _request: Request,
    response: Response,
    _next: NextFunction,
  ) => {
    if (response.headersSent) return;
    response.status(400).json({ error: "invalid_body" });
  });

  return app;
}

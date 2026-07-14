import { createHash } from "node:crypto";
import express, {
  type Express,
  type NextFunction,
  type Request,
  type RequestHandler,
  type Response,
} from "express";
import type { Pool, PoolClient } from "pg";
import type { OrderResponse } from "../starter/orders_api";

interface OrderInput {
  quantity: number;
  sku: string;
}

interface ExistingIdempotencyRecord {
  request_sha256: string;
  response_body: unknown;
  response_status: number | null;
  state: string;
}

function isOrderInput(value: unknown): value is OrderInput {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    return false;
  }
  const record = value as Record<string, unknown>;
  const keys = Object.keys(record).sort();
  const sku = record.sku;
  const quantity = record.quantity;
  if (keys.length !== 2 || keys[0] !== "quantity" || keys[1] !== "sku") {
    return false;
  }
  return (
    typeof sku === "string" &&
    sku.length >= 1 &&
    sku.length <= 100 &&
    sku.trim() === sku &&
    typeof quantity === "number" &&
    Number.isSafeInteger(quantity) &&
    quantity >= 1 &&
    quantity <= 1000
  );
}

function isOrderResponse(value: unknown): value is OrderResponse {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    return false;
  }
  const record = value as Record<string, unknown>;
  return (
    typeof record.id === "string" &&
    typeof record.sku === "string" &&
    Number.isSafeInteger(record.quantity)
  );
}

function isPositiveDecimal(value: string | undefined): value is string {
  return typeof value === "string" && /^[1-9][0-9]*$/u.test(value);
}

function isValidIdempotencyKey(value: string | undefined): value is string {
  return (
    typeof value === "string" &&
    value.length >= 1 &&
    value.length <= 128 &&
    value.trim() === value
  );
}

async function rollback(client: PoolClient): Promise<void> {
  try {
    await client.query("ROLLBACK");
  } catch {
    // The original database failure remains the response cause.
  }
}

function parserStatus(error: unknown): number | null {
  if (!error || typeof error !== "object") return null;
  return (error as { status?: unknown }).status === 400 ? 400 : null;
}

export function createOrdersApp(
  pool: Pool,
  authenticate: RequestHandler,
): Express {
  const app = express();
  app.use(authenticate);
  app.use(express.json({
    limit: "16kb",
    strict: true,
    verify(request: Request, _response, buffer: Buffer) {
      request.rawBody = Buffer.from(buffer);
    },
  }));

  app.post("/orders", async (request, response) => {
    const idempotencyKey = request.get("Idempotency-Key");
    if (!isPositiveDecimal(request.authenticatedUserId)) {
      return response.status(401).json({ error: "unauthenticated" });
    }
    if (!isValidIdempotencyKey(idempotencyKey)) {
      return response.status(400).json({ error: "invalid_idempotency_key" });
    }
    if (!Buffer.isBuffer(request.rawBody) || !isOrderInput(request.body)) {
      return response.status(400).json({ error: "invalid_body" });
    }

    const userId = request.authenticatedUserId;
    const requestSha256 = createHash("sha256")
      .update(request.rawBody)
      .digest("hex");
    const order = request.body;
    let client: PoolClient | null = null;
    let transactionOpen = false;

    try {
      client = await pool.connect();
      await client.query("BEGIN");
      transactionOpen = true;
      const claimed = await client.query(
        `INSERT INTO public.order_idempotency (
           user_id, idempotency_key, request_sha256, state
         ) VALUES ($1, $2, $3, 'processing')
         ON CONFLICT (user_id, idempotency_key) DO NOTHING
         RETURNING user_id`,
        [userId, idempotencyKey, requestSha256],
      );

      if (claimed.rowCount === 0) {
        const existing = await client.query<ExistingIdempotencyRecord>(
          `SELECT request_sha256, state, response_status, response_body
             FROM public.order_idempotency
            WHERE user_id = $1 AND idempotency_key = $2
            FOR UPDATE`,
          [userId, idempotencyKey],
        );
        if (existing.rowCount !== 1) {
          throw new Error("idempotency record disappeared");
        }
        const record = existing.rows[0];
        if (record.request_sha256 !== requestSha256) {
          await rollback(client);
          transactionOpen = false;
          return response.status(409).json({
            error: "idempotency_key_reused",
          });
        }
        if (record.state !== "completed") {
          await rollback(client);
          transactionOpen = false;
          return response.status(409).json({ error: "request_in_progress" });
        }
        if (
          record.response_status !== 201 ||
          !isOrderResponse(record.response_body)
        ) {
          throw new Error("completed idempotency record is invalid");
        }
        await client.query("COMMIT");
        transactionOpen = false;
        return response.status(record.response_status).json(record.response_body);
      }

      const insertedOrder = await client.query<OrderResponse>(
        `INSERT INTO public.orders (user_id, sku, quantity)
         VALUES ($1, $2, $3)
         RETURNING id::text AS id, sku, quantity`,
        [userId, order.sku, order.quantity],
      );
      if (insertedOrder.rowCount !== 1) {
        throw new Error("order creation did not return one row");
      }
      const orderResponse = insertedOrder.rows[0];
      const completed = await client.query(
        `UPDATE public.order_idempotency
            SET state = 'completed',
                order_id = $3,
                response_status = 201,
                response_body = $4::jsonb,
                completed_at = clock_timestamp()
          WHERE user_id = $1 AND idempotency_key = $2`,
        [
          userId,
          idempotencyKey,
          orderResponse.id,
          JSON.stringify(orderResponse),
        ],
      );
      if (completed.rowCount !== 1) {
        throw new Error("idempotency completion did not update one row");
      }
      await client.query("COMMIT");
      transactionOpen = false;
      return response.status(201).json(orderResponse);
    } catch {
      if (client && transactionOpen) await rollback(client);
      return response.status(500).json({ error: "internal_error" });
    } finally {
      client?.release();
    }
  });

  app.use((
    error: unknown,
    _request: Request,
    response: Response,
    _next: NextFunction,
  ) => {
    if (response.headersSent) return;
    if (parserStatus(error) === 400) {
      response.status(400).json({ error: "invalid_body" });
      return;
    }
    response.status(500).json({ error: "internal_error" });
  });

  return app;
}

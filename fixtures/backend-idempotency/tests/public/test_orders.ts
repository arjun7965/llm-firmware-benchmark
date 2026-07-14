import assert from "node:assert/strict";
import { createHash, randomUUID } from "node:crypto";
import { readFileSync, rmSync } from "node:fs";
import {
  createServer,
  request as sendRequest,
  type Server,
} from "node:http";
import { tmpdir } from "node:os";
import { join } from "node:path";
import type { RequestHandler } from "express";
import { Pool } from "pg";
import { createOrdersApp } from "../../generated/orders";

interface HttpReply {
  json: unknown;
  status: number;
  text: string;
}

interface OrderReply {
  id: string;
  quantity: number;
  sku: string;
}

function asObject(value: unknown): Record<string, unknown> {
  assert.ok(value && typeof value === "object" && !Array.isArray(value));
  return value as Record<string, unknown>;
}

function asOrderReply(reply: HttpReply): OrderReply {
  assert.equal(reply.status, 201);
  const value = asObject(reply.json);
  assert.equal(typeof value.id, "string");
  assert.equal(typeof value.sku, "string");
  assert.equal(typeof value.quantity, "number");
  return value as unknown as OrderReply;
}

function errorCode(reply: HttpReply): string {
  return String(asObject(reply.json).error);
}

function postOrder(socketPath: string, {
  body,
  idempotencyKey,
  userId = "101",
}: {
  body: string;
  idempotencyKey?: string;
  userId?: string | null;
}): Promise<HttpReply> {
  return new Promise((resolve, reject) => {
    const headers: Record<string, string> = {
      "content-length": String(Buffer.byteLength(body)),
      "content-type": "application/json",
    };
    if (idempotencyKey !== undefined) {
      headers["idempotency-key"] = idempotencyKey;
    }
    if (userId !== null) headers["x-test-user-id"] = userId;
    const request = sendRequest({
      socketPath,
      method: "POST",
      path: "/orders",
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
          // Status assertions still expose an unexpected non-JSON response.
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
  if (!schemaPath) throw new Error("expected schema path");
  const pool = new Pool({ max: 8 });
  const authenticate: RequestHandler = (request, response, next) => {
    const userId = request.get("x-test-user-id");
    if (!userId || !/^[1-9][0-9]*$/u.test(userId)) {
      response.status(401).json({ error: "unauthenticated" });
      return;
    }
    request.authenticatedUserId = userId;
    next();
  };
  const socketPath = join(tmpdir(), `orders-${randomUUID()}.sock`);
  const server = createServer(createOrdersApp(pool, authenticate));

  try {
    await pool.query(readFileSync(schemaPath, "utf8"));
    const role = await pool.query<{
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
    await listen(server, socketPath);

    const firstBody = '{"sku":"pencil","quantity":2}';
    const first = asOrderReply(await postOrder(socketPath, {
      body: firstBody,
      idempotencyKey: "first-order",
    }));
    assert.deepEqual(first, {
      id: first.id,
      sku: "pencil",
      quantity: 2,
    });
    const replay = await postOrder(socketPath, {
      body: firstBody,
      idempotencyKey: "first-order",
    });
    assert.equal(replay.status, 201);
    assert.deepEqual(replay.json, first);
    assert.equal(await count(
      pool,
      "SELECT count(*)::integer AS count FROM public.orders WHERE user_id = 101",
    ), 1);

    const concurrentBody = '{"sku":"notebook","quantity":3}';
    const concurrent = await Promise.all(
      Array.from({ length: 8 }, () => postOrder(socketPath, {
        body: concurrentBody,
        idempotencyKey: "concurrent-order",
        userId: "202",
      })),
    );
    const concurrentOrder = asOrderReply(concurrent[0]);
    for (const reply of concurrent) {
      assert.equal(reply.status, 201);
      assert.deepEqual(reply.json, concurrentOrder);
    }
    assert.equal(await count(
      pool,
      "SELECT count(*)::integer AS count FROM public.orders WHERE user_id = 202",
    ), 1);

    const changedFirst = await postOrder(socketPath, {
      body: '{"sku":"eraser","quantity":1}',
      idempotencyKey: "changed-body",
      userId: "303",
    });
    asOrderReply(changedFirst);
    const changed = await postOrder(socketPath, {
      body: '{"sku":"marker","quantity":1}',
      idempotencyKey: "changed-body",
      userId: "303",
    });
    assert.equal(changed.status, 409);
    assert.equal(errorCode(changed), "idempotency_key_reused");
    assert.equal(await count(
      pool,
      "SELECT count(*)::integer AS count FROM public.orders WHERE user_id = 303",
    ), 1);

    const rawBody = '{"sku":"paper","quantity":1}';
    asOrderReply(await postOrder(socketPath, {
      body: rawBody,
      idempotencyKey: "raw-body",
      userId: "404",
    }));
    const changedWhitespace = await postOrder(socketPath, {
      body: '{"sku":"paper", "quantity":1}',
      idempotencyKey: "raw-body",
      userId: "404",
    });
    assert.equal(changedWhitespace.status, 409);
    assert.equal(errorCode(changedWhitespace), "idempotency_key_reused");

    const sharedKey = "different-users";
    const sharedBody = '{"sku":"folder","quantity":4}';
    const firstUser = asOrderReply(await postOrder(socketPath, {
      body: sharedBody,
      idempotencyKey: sharedKey,
      userId: "501",
    }));
    const secondUser = asOrderReply(await postOrder(socketPath, {
      body: sharedBody,
      idempotencyKey: sharedKey,
      userId: "502",
    }));
    assert.notEqual(firstUser.id, secondUser.id);
    assert.equal(await count(
      pool,
      "SELECT count(*)::integer AS count FROM public.orders WHERE user_id IN (501, 502)",
    ), 2);

    const inProgressBody = '{"sku":"pending","quantity":1}';
    await pool.query(
      `INSERT INTO public.order_idempotency (
         user_id, idempotency_key, request_sha256, state
       ) VALUES ($1, $2, $3, 'processing')`,
      [
        "601",
        "in-progress",
        createHash("sha256").update(inProgressBody).digest("hex"),
      ],
    );
    const inProgress = await postOrder(socketPath, {
      body: inProgressBody,
      idempotencyKey: "in-progress",
      userId: "601",
    });
    assert.equal(inProgress.status, 409);
    assert.equal(errorCode(inProgress), "request_in_progress");
    assert.equal(await count(
      pool,
      "SELECT count(*)::integer AS count FROM public.orders WHERE user_id = 601",
    ), 0);

    const missingKey = await postOrder(socketPath, {
      body: '{"sku":"invalid","quantity":1}',
      userId: "701",
    });
    assert.equal(missingKey.status, 400);
    const invalidQuantity = await postOrder(socketPath, {
      body: '{"sku":"invalid","quantity":0}',
      idempotencyKey: "invalid-quantity",
      userId: "701",
    });
    assert.equal(invalidQuantity.status, 400);
    const unknownField = await postOrder(socketPath, {
      body: '{"sku":"invalid","quantity":1,"extra":true}',
      idempotencyKey: "unknown-field",
      userId: "701",
    });
    assert.equal(unknownField.status, 400);
    const unauthenticated = await postOrder(socketPath, {
      body: '{"sku":"invalid","quantity":1}',
      idempotencyKey: "missing-user",
      userId: null,
    });
    assert.equal(unauthenticated.status, 401);

    await pool.query(
      `CREATE FUNCTION public.reject_atomic_order()
       RETURNS trigger
       LANGUAGE plpgsql
       AS $function$
       BEGIN
         IF NEW.sku = 'atomic-failure' THEN
           RAISE EXCEPTION 'forced order insert failure';
         END IF;
         RETURN NEW;
       END;
       $function$`,
    );
    await pool.query(
      `CREATE TRIGGER reject_atomic_order
       BEFORE INSERT ON public.orders
       FOR EACH ROW EXECUTE FUNCTION public.reject_atomic_order()`,
    );
    const atomicFailure = await postOrder(socketPath, {
      body: '{"sku":"atomic-failure","quantity":1}',
      idempotencyKey: "atomic-failure",
      userId: "801",
    });
    assert.equal(atomicFailure.status, 500, atomicFailure.text);
    assert.equal(await count(
      pool,
      "SELECT count(*)::integer AS count FROM public.orders WHERE user_id = 801",
    ), 0);
    assert.equal(await count(
      pool,
      `SELECT count(*)::integer AS count
         FROM public.order_idempotency
        WHERE user_id = 801 AND idempotency_key = 'atomic-failure'`,
    ), 0);
    await pool.query("DROP TRIGGER reject_atomic_order ON public.orders");
    await pool.query("DROP FUNCTION public.reject_atomic_order()");
  } finally {
    if (server.listening) await close(server);
    rmSync(socketPath, { force: true });
    await pool.end();
  }
}

main().then(
  () => console.log("backend-idempotency tests passed"),
  (error: unknown) => {
    console.error(error);
    process.exitCode = 1;
  },
);

import type { Express, RequestHandler } from "express";
import type { Pool } from "pg";

declare global {
  namespace Express {
    interface Request {
      authenticatedUserId?: string;
      rawBody?: Buffer;
    }
  }
}

export interface OrderResponse {
  id: string;
  sku: string;
  quantity: number;
}

export type OrdersAppFactory = (
  pool: Pool,
  authenticate: RequestHandler,
) => Express;

import type { Express } from "express";
import type { Pool } from "pg";

export interface WebhookSuccess {
  status: "accepted";
}

export type WebhookAppFactory = (
  pool: Pool,
  secrets: readonly string[],
  now?: () => number,
) => Express;

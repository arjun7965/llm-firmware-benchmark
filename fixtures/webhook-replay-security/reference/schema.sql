CREATE TABLE public.webhook_events (
  webhook_id text PRIMARY KEY CHECK (
    char_length(webhook_id) BETWEEN 1 AND 128
    AND webhook_id = btrim(webhook_id)
  ),
  authenticated_sha256 char(64) NOT NULL CHECK (
    authenticated_sha256 ~ '^[0-9a-f]{64}$'
  ),
  signed_timestamp bigint NOT NULL CHECK (signed_timestamp >= 0),
  payload jsonb NOT NULL,
  received_at timestamptz NOT NULL DEFAULT clock_timestamp()
);

CREATE TABLE public.webhook_outbox (
  id bigserial PRIMARY KEY,
  webhook_id text NOT NULL UNIQUE REFERENCES public.webhook_events(webhook_id),
  topic text NOT NULL CHECK (topic = 'webhook.received'),
  payload jsonb NOT NULL,
  created_at timestamptz NOT NULL DEFAULT clock_timestamp()
);

CREATE TABLE public.orders (
  id bigserial PRIMARY KEY,
  user_id bigint NOT NULL CHECK (user_id > 0),
  sku text NOT NULL CHECK (
    char_length(sku) BETWEEN 1 AND 100
    AND sku = btrim(sku)
  ),
  quantity integer NOT NULL CHECK (quantity BETWEEN 1 AND 1000),
  created_at timestamptz NOT NULL DEFAULT clock_timestamp()
);

CREATE TABLE public.order_idempotency (
  user_id bigint NOT NULL CHECK (user_id > 0),
  idempotency_key text NOT NULL CHECK (
    char_length(idempotency_key) BETWEEN 1 AND 128
    AND idempotency_key = btrim(idempotency_key)
  ),
  request_sha256 char(64) NOT NULL CHECK (
    request_sha256 ~ '^[0-9a-f]{64}$'
  ),
  state text NOT NULL CHECK (state IN ('processing', 'completed')),
  order_id bigint UNIQUE REFERENCES public.orders(id),
  response_status smallint CHECK (response_status = 201),
  response_body jsonb,
  created_at timestamptz NOT NULL DEFAULT clock_timestamp(),
  completed_at timestamptz,
  PRIMARY KEY (user_id, idempotency_key),
  CHECK (
    (
      state = 'processing'
      AND order_id IS NULL
      AND response_status IS NULL
      AND response_body IS NULL
      AND completed_at IS NULL
    )
    OR
    (
      state = 'completed'
      AND order_id IS NOT NULL
      AND response_status = 201
      AND response_body IS NOT NULL
      AND completed_at IS NOT NULL
    )
  )
);

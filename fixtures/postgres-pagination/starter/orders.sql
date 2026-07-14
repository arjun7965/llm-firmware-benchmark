CREATE TABLE public.orders (
  id bigint PRIMARY KEY,
  tenant_id bigint NOT NULL,
  created_at timestamptz NOT NULL,
  status text NOT NULL CHECK (
    status IN ('pending', 'paid', 'shipped', 'cancelled')
  ),
  total numeric(12, 2) NOT NULL CHECK (total >= 0)
);

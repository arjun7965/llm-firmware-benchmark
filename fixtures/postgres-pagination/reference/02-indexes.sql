CREATE INDEX orders_tenant_status_created_id_idx
ON public.orders (tenant_id, status, created_at DESC, id DESC)
INCLUDE (total);

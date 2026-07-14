CREATE FUNCTION pg_temp.assert_true(p_condition boolean, p_message text)
RETURNS void
LANGUAGE plpgsql
AS $function$
BEGIN
  IF p_condition IS DISTINCT FROM true THEN
    RAISE EXCEPTION 'assertion failed: %', p_message;
  END IF;
END;
$function$;

SELECT pg_temp.assert_true(
  current_user = 'benchmark' AND NOT (
    SELECT rolsuper
    FROM pg_roles
    WHERE rolname = current_user
  ),
  'candidate SQL must run as the non-superuser benchmark role'
);

INSERT INTO public.orders (id, tenant_id, created_at, status, total) VALUES
  (101, 1, '2026-01-01 12:00:00+00', 'pending', 10.00),
  (102, 1, '2026-01-02 12:00:00+00', 'cancelled', 20.00),
  (103, 1, '2026-01-02 12:00:00+00', 'paid', 30.00),
  (104, 1, '2026-01-03 12:00:00+00', 'pending', 40.00),
  (105, 1, '2026-01-03 12:00:00+00', 'paid', 50.00),
  (201, 2, '2026-01-04 12:00:00+00', 'paid', 60.00),
  (202, 2, '2026-01-02 12:00:00+00', 'pending', 70.00);

SELECT pg_temp.assert_true(
  ARRAY(
    SELECT id
    FROM public.orders_page(1, ARRAY['pending', 'paid'], NULL, 3)
  ) = ARRAY[105, 104, 103]::bigint[],
  'first page must use descending timestamp and id order'
);

SELECT pg_temp.assert_true(
  ARRAY(
    SELECT id
    FROM public.orders_page(
      1,
      ARRAY['paid', 'pending'],
      public.orders_cursor(
        1,
        ARRAY['pending', 'paid'],
        '2026-01-03 12:00:00+00',
        105
      ),
      2
    )
  ) = ARRAY[104, 103]::bigint[],
  'equal timestamps must continue with the id tie breaker'
);

CREATE TEMP TABLE first_page AS
SELECT *
FROM public.orders_page(1, ARRAY['pending', 'paid'], NULL, 3);

INSERT INTO public.orders (id, tenant_id, created_at, status, total)
VALUES (106, 1, '2026-01-05 12:00:00+00', 'paid', 80.00);

SELECT pg_temp.assert_true(
  ARRAY(
    SELECT id
    FROM public.orders_page(
      1,
      ARRAY['pending', 'paid'],
      public.orders_cursor(
        1,
        ARRAY['pending', 'paid'],
        (SELECT created_at FROM first_page ORDER BY created_at, id LIMIT 1),
        (SELECT id FROM first_page ORDER BY created_at, id LIMIT 1)
      ),
      50
    )
  ) = ARRAY[101]::bigint[],
  'next page must not duplicate rows or admit newer inserts'
);

SELECT pg_temp.assert_true(
  NOT EXISTS (
    SELECT 1
    FROM public.orders_page(1, ARRAY['paid'], NULL, 100)
    WHERE id >= 200
  ),
  'tenant isolation must be enforced in the query'
);

SELECT pg_temp.assert_true(
  NOT EXISTS (
    SELECT 1
    FROM public.orders_page(1, ARRAY['paid'], NULL, 100)
    WHERE status <> 'paid'
  ),
  'status filters must be enforced in the query'
);

SELECT pg_temp.assert_true(
  public.orders_cursor(
    1,
    ARRAY['pending', 'paid', 'paid'],
    '2026-01-03 12:00:00+00',
    104
  ) -> 'statuses' = '["paid", "pending"]'::jsonb,
  'cursor status filters must be canonical and duplicate free'
);

DO $block$
BEGIN
  BEGIN
    PERFORM * FROM public.orders_page(
      2,
      ARRAY['pending', 'paid'],
      public.orders_cursor(
        1,
        ARRAY['pending', 'paid'],
        '2026-01-03 12:00:00+00',
        104
      ),
      50
    );
    RAISE EXCEPTION 'tenant-bound cursor was accepted';
  EXCEPTION WHEN SQLSTATE '22023' THEN
    NULL;
  END;

  BEGIN
    PERFORM * FROM public.orders_page(
      1,
      ARRAY['paid'],
      public.orders_cursor(
        1,
        ARRAY['pending', 'paid'],
        '2026-01-03 12:00:00+00',
        104
      ),
      50
    );
    RAISE EXCEPTION 'filter-bound cursor was accepted';
  EXCEPTION WHEN SQLSTATE '22023' THEN
    NULL;
  END;

  BEGIN
    PERFORM * FROM public.orders_page(
      1,
      ARRAY['paid'],
      '{"tenant_id":1,"statuses":["paid"],"created_at":"2026-01-03T12:00:00Z"}'::jsonb,
      50
    );
    RAISE EXCEPTION 'partial cursor was accepted';
  EXCEPTION WHEN SQLSTATE '22023' THEN
    NULL;
  END;

  BEGIN
    PERFORM * FROM public.orders_page(
      1,
      ARRAY['paid'],
      '{"tenant_id":1,"statuses":["paid"],"created_at":"not-a-date","id":104}'::jsonb,
      50
    );
    RAISE EXCEPTION 'malformed cursor was accepted';
  EXCEPTION WHEN SQLSTATE '22023' THEN
    NULL;
  END;

  BEGIN
    PERFORM * FROM public.orders_page(1, ARRAY['paid'], NULL, 101);
    RAISE EXCEPTION 'oversized page was accepted';
  EXCEPTION WHEN SQLSTATE '22023' THEN
    NULL;
  END;

  BEGIN
    PERFORM public.orders_cursor(
      1,
      ARRAY['unknown'],
      '2026-01-03 12:00:00+00',
      104
    );
    RAISE EXCEPTION 'unknown status was accepted';
  EXCEPTION WHEN SQLSTATE '22023' THEN
    NULL;
  END;
END;
$block$;

SELECT pg_temp.assert_true(
  to_regclass('public.orders_tenant_status_created_id_idx') IS NOT NULL,
  'the pagination index must exist'
);

SELECT pg_temp.assert_true(
  pg_get_indexdef('public.orders_tenant_status_created_id_idx'::regclass)
    LIKE '%(tenant_id, status, created_at DESC, id DESC) INCLUDE (total)',
  'index columns, ordering, and included total must match the contract'
);

SELECT 'postgres-pagination tests passed' AS result;

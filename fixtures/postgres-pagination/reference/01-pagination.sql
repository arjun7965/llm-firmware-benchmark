CREATE FUNCTION public.canonical_order_statuses(p_statuses text[])
RETURNS text[]
LANGUAGE sql
IMMUTABLE
PARALLEL SAFE
AS $function$
  SELECT array_agg(status ORDER BY status)
  FROM (
    SELECT DISTINCT value AS status
    FROM unnest(p_statuses) AS values(value)
  ) AS canonical;
$function$;

CREATE FUNCTION public.orders_cursor(
  p_tenant_id bigint,
  p_statuses text[],
  p_created_at timestamptz,
  p_id bigint
)
RETURNS jsonb
LANGUAGE plpgsql
IMMUTABLE
PARALLEL SAFE
AS $function$
DECLARE
  v_statuses text[];
BEGIN
  IF p_tenant_id IS NULL OR p_tenant_id <= 0 THEN
    RAISE EXCEPTION 'tenant_id must be positive' USING ERRCODE = '22023';
  END IF;
  IF p_statuses IS NULL OR cardinality(p_statuses) = 0 OR
      array_position(p_statuses, NULL) IS NOT NULL THEN
    RAISE EXCEPTION 'statuses must be a nonempty array without nulls'
      USING ERRCODE = '22023';
  END IF;
  v_statuses := public.canonical_order_statuses(p_statuses);
  IF NOT v_statuses <@ ARRAY['cancelled', 'paid', 'pending', 'shipped']::text[]
      THEN
    RAISE EXCEPTION 'statuses contain an unknown value'
      USING ERRCODE = '22023';
  END IF;
  IF p_created_at IS NULL OR p_id IS NULL THEN
    RAISE EXCEPTION 'cursor anchor must be complete' USING ERRCODE = '22023';
  END IF;
  RETURN jsonb_build_object(
    'tenant_id', p_tenant_id,
    'statuses', to_jsonb(v_statuses),
    'created_at', to_jsonb(p_created_at),
    'id', p_id
  );
END;
$function$;

CREATE FUNCTION public.orders_page(
  p_tenant_id bigint,
  p_statuses text[],
  p_cursor jsonb DEFAULT NULL,
  p_limit integer DEFAULT 50
)
RETURNS TABLE (
  id bigint,
  created_at timestamptz,
  status text,
  total numeric
)
LANGUAGE plpgsql
STABLE
PARALLEL SAFE
AS $function$
DECLARE
  v_statuses text[];
  v_cursor_statuses text[];
  v_cursor_tenant bigint;
  v_created_at timestamptz;
  v_id bigint;
  v_keys text[];
BEGIN
  IF p_tenant_id IS NULL OR p_tenant_id <= 0 THEN
    RAISE EXCEPTION 'tenant_id must be positive' USING ERRCODE = '22023';
  END IF;
  IF p_statuses IS NULL OR cardinality(p_statuses) = 0 OR
      array_position(p_statuses, NULL) IS NOT NULL THEN
    RAISE EXCEPTION 'statuses must be a nonempty array without nulls'
      USING ERRCODE = '22023';
  END IF;
  v_statuses := public.canonical_order_statuses(p_statuses);
  IF NOT v_statuses <@ ARRAY['cancelled', 'paid', 'pending', 'shipped']::text[]
      THEN
    RAISE EXCEPTION 'statuses contain an unknown value'
      USING ERRCODE = '22023';
  END IF;
  IF p_limit IS NULL OR p_limit < 1 OR p_limit > 100 THEN
    RAISE EXCEPTION 'limit must be between 1 and 100'
      USING ERRCODE = '22023';
  END IF;

  IF p_cursor IS NOT NULL THEN
    IF jsonb_typeof(p_cursor) <> 'object' THEN
      RAISE EXCEPTION 'cursor must be an object' USING ERRCODE = '22023';
    END IF;
    SELECT array_agg(key ORDER BY key)
    INTO v_keys
    FROM jsonb_object_keys(p_cursor) AS keys(key);
    IF v_keys IS DISTINCT FROM
        ARRAY['created_at', 'id', 'statuses', 'tenant_id']::text[] THEN
      RAISE EXCEPTION 'cursor fields are invalid' USING ERRCODE = '22023';
    END IF;
    IF jsonb_typeof(p_cursor -> 'tenant_id') <> 'number' OR
        jsonb_typeof(p_cursor -> 'statuses') <> 'array' OR
        jsonb_typeof(p_cursor -> 'created_at') <> 'string' OR
        jsonb_typeof(p_cursor -> 'id') <> 'number' THEN
      RAISE EXCEPTION 'cursor field types are invalid' USING ERRCODE = '22023';
    END IF;
    BEGIN
      v_cursor_tenant := (p_cursor ->> 'tenant_id')::bigint;
      v_created_at := (p_cursor ->> 'created_at')::timestamptz;
      v_id := (p_cursor ->> 'id')::bigint;
      SELECT array_agg(value ORDER BY value)
      INTO v_cursor_statuses
      FROM jsonb_array_elements_text(p_cursor -> 'statuses') AS values(value);
    EXCEPTION WHEN OTHERS THEN
      RAISE EXCEPTION 'cursor values are invalid' USING ERRCODE = '22023';
    END;
    IF v_cursor_tenant IS DISTINCT FROM p_tenant_id OR
        v_cursor_statuses IS DISTINCT FROM v_statuses THEN
      RAISE EXCEPTION 'cursor does not match tenant and filters'
        USING ERRCODE = '22023';
    END IF;
  END IF;

  RETURN QUERY
  SELECT o.id, o.created_at, o.status, o.total
  FROM public.orders AS o
  WHERE o.tenant_id = p_tenant_id
    AND o.status = ANY(v_statuses)
    AND (
      v_created_at IS NULL OR
      (o.created_at, o.id) < (v_created_at, v_id)
    )
  ORDER BY o.created_at DESC, o.id DESC
  LIMIT p_limit;
END;
$function$;

# PostgreSQL Pagination

## Objective

Assess stable keyset pagination and index design for a large, tenant-isolated
PostgreSQL workload.

## Scoring

- 3 points — The continuation predicate exactly matches descending timestamp and ID order.
- 2 points — Cursor contents and validation preserve tenant and filter isolation.
- 2 points — Index recommendations address equality prefixes and arbitrary status arrays.
- 2 points — Equal timestamps, inserts between pages, and consistency tradeoffs are explained.
- 1 point — SQL and API examples clearly distinguish first and subsequent pages.

A cursor containing only `created_at` is not stable when timestamps tie.

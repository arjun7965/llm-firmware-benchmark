# PostgreSQL Pagination

## Objective

Assess stable keyset pagination and index design for a large, tenant-isolated
PostgreSQL workload.

The active fixture supplies the `orders` table and requires the ordered
`01-pagination.sql` and `02-indexes.sql` bundle. The first file defines strict
cursor construction and page functions; the second defines the covering
status-prefixed index. Every compile and test phase runs against a fresh
PostgreSQL 16.9 cluster with no TCP listener.

## Scoring

- 3 points — The continuation predicate exactly matches descending timestamp and ID order.
- 2 points — Cursor contents and validation preserve tenant and filter isolation.
- 2 points — Index recommendations address equality prefixes and arbitrary status arrays.
- 2 points — Equal timestamps, inserts between pages, and consistency tradeoffs are explained.
- 1 point — SQL and API examples clearly distinguish first and subsequent pages.

A cursor containing only `created_at` is not stable when timestamps tie.

The public tests cover first and next pages, equal timestamps, inserts newer
than the cursor anchor, tenant and filter binding, malformed cursors, page-size
bounds, status canonicalization, and index shape. The trusted reference and
twelve controlled defects are calibrated with:

```bash
npm run fixture:postgres-pagination:self-test
```

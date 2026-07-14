# Public tests

The SQL suite checks first and subsequent pages, equal-timestamp tie breaking,
tenant and status isolation, inserts between pages, strict cursor validation,
page bounds, canonical filters, and the declared index shape. Each invocation
runs against a newly initialized PostgreSQL 16.9 cluster.

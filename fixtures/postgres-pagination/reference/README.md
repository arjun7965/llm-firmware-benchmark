# Trusted reference

`01-pagination.sql` implements strict tenant/filter-bound cursors and
descending row-value keyset pagination. `02-indexes.sql` supplies the covering
status-prefixed index. The reference and controlled defects are calibrated by
`../scripts/verify-reference.mjs` against a fresh PostgreSQL cluster per
compile or test phase.

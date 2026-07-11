# Trusted Reference

`server.go` serializes queue closure against handler sends with an admission
read/write lock. It never closes the queue until admission is disabled, then
lets exactly four workers drain accepted jobs. Processor cancellation is
reserved for an expired shutdown deadline.

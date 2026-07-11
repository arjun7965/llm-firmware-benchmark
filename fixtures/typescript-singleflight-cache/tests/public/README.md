# Public Tests

`test_cache.ts` uses an injected fake clock, deferred promises, and real
`AbortController` instances. It dynamically imports the candidate after
installing an exit guard so import-time successful exits cannot bypass tests.

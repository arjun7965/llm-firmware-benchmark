# Public Tests

`test_pool.py` loads the extracted candidate directly and exercises each
concurrency scenario in a bounded subprocess. This keeps deadlocking mutants
and leaked worker threads from hanging the validator process. Scenario threads
use 256 KiB stacks so the race tests remain inside the profile's address-space
limit without reducing their concurrency.

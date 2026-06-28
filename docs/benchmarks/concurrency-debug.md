# Concurrency Debug

## Objective

Assess diagnosis and repair of shutdown races, deadlocks, and task accounting
in a Python worker pool.

## Scoring

- 3 points — Shutdown completes accepted work without deadlock or stranded workers.
- 2 points — Queue accounting remains correct when tasks raise exceptions.
- 2 points — Submission races, rejection, and repeated shutdown calls are synchronized.
- 2 points — Invalid worker counts and shutdown from a worker have defined behavior.
- 1 point — Tests deterministically exercise the critical races and exception path.

Polling an unsynchronized stop flag or omitting one `task_done()` path cannot
receive full shutdown correctness credit.

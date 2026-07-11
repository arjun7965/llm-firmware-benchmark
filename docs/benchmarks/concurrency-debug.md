# Concurrency Debug

## Objective

Assess diagnosis and repair of shutdown races, deadlocks, and task accounting
in a Python worker pool.

The scaffold contract is under `fixtures/concurrency-debug/`. Candidates
provide one module defining `Pool`; validator-owned tests load that module and
run each bounded concurrency scenario in an isolated subprocess so a broken
shutdown cannot hang the validator.

## Scoring

- 3 points — Shutdown completes accepted work without deadlock or stranded workers.
- 2 points — Queue accounting remains correct when tasks raise exceptions.
- 2 points — Submission races, rejection, and repeated shutdown calls are synchronized.
- 2 points — Invalid worker counts and shutdown from a worker have defined behavior.
- 1 point — Tests deterministically exercise the critical races and exception path.

Polling an unsynchronized stop flag or omitting one `task_done()` path cannot
receive full shutdown correctness credit.

The trusted reference and twelve controlled mutations are calibrated with:

```bash
npm run fixture:concurrency:self-test
```

The fixture remains a scaffold until the tests execute under the exact Python
3.12.11 profile inside the sandbox namespace.

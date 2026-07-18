# Answer Contracts

## Decision

Fixture-backed tasks use a single-source-file answer contract by default.
Multi-file answers are opt-in through the validated `markdown-file-bundle`
contract. Existing single-file prompts and result interpretation do not change
as part of this decision. Single-file extraction enforces the documented
one-block rule by rejecting every additional fenced block.

The current single-file contract is identified by the fixture manifest answer
format `markdown-fenced-code`. A model returns exactly one nonempty fenced code
block with the declared language. The extractor writes that block to the one
manifest-owned `generated/` output path. Explanatory prose may appear outside
the block. Multiple matching blocks, model-selected paths, patches, archives,
and undeclared outputs are invalid.

Validator-owned public tests remain separate fixture assets. A request to
describe or propose tests does not by itself require the model to return test
files.

## Task Decisions

The **current mode** describes behavior in the repository today. `scoringMode`
in `tasks.json` separately records whether a task has deterministic fixture
evidence or is intentionally rubric-only. A rubric-only task has no executable
answer contract or fixture until it is reclassified and calibrated under the
[rubric-only task policy](rubric-only-tasks.md).

| Task | Current mode | Planned executable answer contract | Reason |
| --- | --- | --- | --- |
| `bare-metal-timer` | Single file | Single file | One implementation unit; mocks and tests are validator-owned |
| `interrupt-vector-configuration` | Single file | Single file | One startup/vector implementation unit; SCB/NVIC mocks and ordering tests are validator-owned |
| `linker-memory-map` | Single file | Single file | One reset-time map implementation unit; opaque linker-symbol and flash/SRAM mocks are validator-owned |
| `i2c-controller-recovery` | Single file | Single file | One I2C controller implementation unit; opaque I2C0 mocks and protocol-ordering tests are validator-owned |
| `gpio-edge-debounce` | Single file | Single file | One GPIO button driver unit; opaque edge/wake and interrupt-mask mocks are validator-owned |
| `adc-threshold-watchdog` | Single file | Single file | One ADC threshold monitor unit; opaque status, data, and interrupt-mask mocks are validator-owned |
| `pwm-synchronized-update` | Single file | Single file | One PWM driver unit; opaque shadow/load, fault, and interrupt-mask mocks are validator-owned |
| `watchdog-window-recovery` | Single file | Single file | One watchdog driver unit; opaque counter, reset-cause, feed, and interrupt-mask mocks are validator-owned |
| `timer-dma-handoff` | Single file | Single file | One timer/DMA ownership driver unit; opaque compare-stream, terminal-status, and interrupt-mask mocks are validator-owned |
| `timer-capture-overflow` | Single file | Single file | One timer capture/compare driver unit; opaque counter, capture, compare, overflow-status, and interrupt-mask mocks are validator-owned |
| `uart-interrupt-driver` | Single file | Single file | One driver implementation unit; UART0 mock and interrupt tests are validator-owned |
| `spi-dma-transfer` | Single file | Single file | One driver implementation unit; SPI0/DMA0 mocks and DMA IRQ tests are validator-owned |
| `can-controller-recovery` | Single file | Single file | One CAN0 controller implementation unit; mailbox, bus-off, and interrupt-mask tests are validator-owned |
| `interrupt-deferred-work` | Single file | Single file | One atomic ISR-to-foreground dispatcher unit; nested-priority latch and interrupt-mask tests are validator-owned |
| `binary-parser` | Single file | Single file | One implementation unit; headers and tests are supplied |
| `embedded-ring-buffer` | Single file | Single file | One implementation unit; headers and tests are supplied |
| `static-memory-pool` | Single file | Single file | One fixed-block allocator implementation unit; embedded storage and tests are supplied |
| `fixed-point-stack-budget` | Single file | Single file | One Q-format worker implementation unit; simulated stack watermark and tests are supplied |
| `dma-cache-coherency` | Single file | Single file | One cache/DMA ordering implementation unit; opaque deterministic cache/DMA mock and tests are supplied |
| `firmware-state-machine` | Single file | Single file | One implementation unit; HAL, mocks, and tests are supplied |
| `rtos-priority-inversion` | Single file | Single file | One implementation unit; RTOS mock and scheduler tests are validator-owned |
| `rtos-periodic-scheduler` | Single file | Single file | One rate-monotonic release implementation unit; RTOS release mock and deadline tests are validator-owned |
| `rtos-queue-semaphore` | Single file | Single file | One producer/worker handoff implementation unit; queue/semaphore mock and FIFO/token tests are validator-owned |
| `rtos-event-flags-deadlock` | Single file | Single file | One supervisor coordination implementation unit; event/mutex mock and lock-order tests are validator-owned |
| `frontend-autocomplete` | Single file | Single file | One default-exported component module; interaction tests are validator-owned |
| `concurrency-debug` | Single file | Single file | One repaired Python module; race tests are validator-owned |
| `testing-property-based` | Single file | Single file | One property-test module importing the supplied implementation |
| `go-graceful-shutdown` | Multi-file | Multi-file | Runnable server code and Go-discoverable `*_test.go` tests require distinct files |
| `rust-stream-decoder` | Single file | Single file | One Rust library module; unit tests are validator-owned |
| `typescript-singleflight-cache` | Single file | Single file | One cache module; fake-clock tests are validator-owned |
| `backend-idempotency` | Multi-file | Multi-file | SQL migration and TypeScript endpoint are independently validated artifacts |
| `postgres-pagination` | Multi-file | Multi-file | SQL/index artifacts and cursor-validation code use distinct files |
| `webhook-replay-security` | Multi-file | Multi-file | SQL migration and TypeScript handler are independently validated artifacts |

Changing a task from its planned contract requires a documented rationale.
Changing an existing prompt to activate executable extraction changes its
prompt hash and invalidates reuse of earlier raw results.

## Multi-File Contract

The `markdown-file-bundle` format does not overload
`markdown-fenced-code`. Its manifest owns a sorted, unique list of relative
file paths and the expected language for each file. Output locations are
derived under `generated/`; the model never chooses filesystem destinations.

A human-readable response uses one file heading followed immediately by one
fenced block:

````markdown
### `migrations/001-create-orders.sql`
```sql
CREATE TABLE orders (...);
```

### `src/orders.ts`
```typescript
export async function createOrder(...) {
  // ...
}
```
````

The implemented parser and writer must:

- accept every declared file exactly once and in manifest order;
- reject missing, duplicate, undeclared, unsafe, absolute, or traversal paths;
- require the exact declared language and reject extra fence metadata;
- enforce per-file and aggregate UTF-8 byte limits;
- validate the complete bundle before writing any file;
- stage writes transactionally so extraction cannot leave a partial bundle;
- preserve the current symlink, containment, overwrite, and NUL-byte checks;
- return per-file byte counts and SHA-256 values plus a canonical bundle
  SHA-256 covering each path, length, and file content; and
- keep all build and test commands as task-declared argv arrays without a
  shell.

The extractor caps the complete response at 1 MiB, each normalized file at
256 KiB, and all normalized bundle contents together at 512 KiB.

Bundle extraction parses and validates every file before staging a replacement
`generated/` directory. Commit uses a directory rename with restoration of the
previous directory on failure, preventing partial bundles. Validation reports
list each generated path, byte length, and content SHA-256. For bundles,
`answerSha256` is a canonical SHA-256 over the ordered file count followed by
each UTF-8 path length and path, then content length and content; all lengths
are unsigned 64-bit big-endian integers. Single-file reports retain the direct
content SHA-256 meaning.

## Comparison Integrity

Compared models must receive an identical prompt and answer contract. Do not
accept a single-file answer from one model and a multi-file answer from another
under the same task revision. Disclose any contract or harness difference, and
recalibrate a task after changing its answer format.

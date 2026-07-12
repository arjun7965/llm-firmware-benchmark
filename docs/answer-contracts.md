# Answer Contracts

## Decision

Fixture-backed tasks use a single-source-file answer contract by default.
Multi-file answers are opt-in and may be activated only after the manifest
schema, extractor, sandbox report, and tests implement the bundle contract
defined below. Existing prompts and result interpretation do not change as
part of this decision. Extraction now enforces the documented one-block rule
by rejecting every additional fenced block.

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

The **current mode** describes behavior in the repository today. Auxiliary
tasks without fixtures remain rubric-only until their planned contract is
implemented and calibrated.

| Task | Current mode | Planned executable answer contract | Reason |
| --- | --- | --- | --- |
| `bare-metal-timer` | Single file | Single file | One implementation unit; mocks and tests are validator-owned |
| `binary-parser` | Single file | Single file | One implementation unit; headers and tests are supplied |
| `embedded-ring-buffer` | Single file | Single file | One implementation unit; headers and tests are supplied |
| `firmware-state-machine` | Single file | Single file | One implementation unit; HAL, mocks, and tests are supplied |
| `frontend-autocomplete` | Rubric-only | Single file | One component module; interaction tests are validator-owned |
| `concurrency-debug` | Single file | Single file | One repaired Python module; race tests are validator-owned |
| `testing-property-based` | Rubric-only | Single file | One property-test module importing the supplied implementation |
| `go-graceful-shutdown` | Rubric-only | Multi-file | Runnable server code and Go-discoverable `*_test.go` tests require distinct files |
| `rust-stream-decoder` | Single file | Single file | One Rust library module; unit tests are validator-owned |
| `typescript-singleflight-cache` | Scaffold | Single file | One cache module; fake-clock tests are validator-owned |
| `backend-idempotency` | Rubric-only | Multi-file | SQL migration and TypeScript endpoint are independently validated artifacts |
| `postgres-pagination` | Scaffold | Multi-file | SQL/index artifacts and cursor-validation code require distinct files |
| `webhook-replay-security` | Rubric-only | Multi-file | SQL migration and TypeScript handler are independently validated artifacts |

Changing a task from its planned contract requires a documented rationale.
Changing an existing prompt to activate executable extraction changes its
prompt hash and invalidates reuse of earlier raw results.

## Future Multi-File Contract

The first multi-file fixture must introduce an explicit answer format such as
`markdown-file-bundle`; it must not overload `markdown-fenced-code`. Its
manifest owns a sorted, unique list of relative file paths and the expected
language for each file. Output locations are derived under `generated/`; the
model never chooses filesystem destinations.

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

Before any multi-file fixture becomes active, bump the affected versioned
schemas, add the new manifest variant, implement extraction and rollback,
define report digest semantics, and add tests for malformed bundles and partial
write failures. Until then, multi-file tasks remain rubric-only.

## Comparison Integrity

Compared models must receive an identical prompt and answer contract. Do not
accept a single-file answer from one model and a multi-file answer from another
under the same task revision. Disclose any contract or harness difference, and
recalibrate a task after changing its answer format.

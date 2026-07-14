import assert from "node:assert/strict";
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import {
  createPostgresqlServiceRunRoot,
  removePostgresqlServiceRunRoot,
  runPostgresqlService,
} from "../src/postgresql-service.mjs";

function invocation(command, args, cwd, timeoutMs = 2_000) {
  return {
    command,
    args,
    cwd,
    env: {},
    timeoutMs,
  };
}

test("service supervisor runs a candidate and tears down its server", (t) => {
  const temporaryRoot = mkdtempSync(join(tmpdir(), "service-test-"));
  t.after(() => rmSync(temporaryRoot, { recursive: true, force: true }));
  const { runRoot, serviceRoot } = createPostgresqlServiceRunRoot({
    temporaryRoot,
  });
  t.after(() => removePostgresqlServiceRunRoot(runRoot));

  const result = runPostgresqlService({
    candidate: invocation(
      "/bin/echo",
      ["candidate passed"],
      temporaryRoot,
    ),
    initialize: invocation("/bin/true", [], temporaryRoot),
    logPath: join(serviceRoot, "service.log"),
    ready: invocation("/bin/true", [], temporaryRoot),
    runRoot,
    shutdownTimeoutMs: 2_000,
    start: invocation("/bin/sleep", ["30"], temporaryRoot),
    startupTimeoutMs: 2_000,
    stop: null,
  });

  assert.equal(result.status, 0);
  assert.equal(result.signal, null);
  assert.equal(result.stdout, "candidate passed\n");
  assert.equal(result.stderr, "");
});

test("service supervisor reports a server spawn failure", (t) => {
  const temporaryRoot = mkdtempSync(join(tmpdir(), "service-test-"));
  t.after(() => rmSync(temporaryRoot, { recursive: true, force: true }));
  const { runRoot, serviceRoot } = createPostgresqlServiceRunRoot({
    temporaryRoot,
  });
  t.after(() => removePostgresqlServiceRunRoot(runRoot));

  const result = runPostgresqlService({
    candidate: invocation("/bin/true", [], temporaryRoot),
    initialize: invocation("/bin/true", [], temporaryRoot),
    logPath: join(serviceRoot, "service.log"),
    ready: invocation("/bin/false", [], temporaryRoot),
    runRoot,
    shutdownTimeoutMs: 1_000,
    start: invocation("/nonexistent/postgres", [], temporaryRoot),
    startupTimeoutMs: 1_000,
    stop: null,
  });

  assert.equal(result.status, 1);
  assert.match(result.stderr, /PostgreSQL did not become ready/u);
  assert.match(result.stderr, /ENOENT/u);
});

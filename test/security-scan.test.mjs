import { execFileSync, spawnSync } from "node:child_process";
import {
  mkdirSync,
  mkdtempSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import test from "node:test";
import assert from "node:assert/strict";

const scanner = fileURLToPath(
  new URL("../scripts/scan-secrets.mjs", import.meta.url),
);

test("Git scan checks ignored files that were force-added", (t) => {
  const root = mkdtempSync(join(tmpdir(), "secret-scan-test-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  mkdirSync(join(root, "results"));
  writeFileSync(join(root, ".gitignore"), "/results/\n");
  const canary = ["never", "publish", "this"].join("-");
  writeFileSync(
    join(root, "results", "raw.txt"),
    [["pass", "word"].join(""), "=", canary].join(""),
  );
  execFileSync("git", ["init", "--quiet"], { cwd: root });
  execFileSync("git", ["add", "-f", "results/raw.txt"], { cwd: root });

  const result = spawnSync(process.execPath, [scanner, "--git"], {
    cwd: root,
    encoding: "utf8",
  });

  assert.equal(result.status, 1);
  assert.match(result.stderr, /potential credential-assignment/);
  assert.equal(result.stderr.includes(canary), false);
});

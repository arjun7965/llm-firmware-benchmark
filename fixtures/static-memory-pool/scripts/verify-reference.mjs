import { spawnSync } from "node:child_process";
import {
  mkdtempSync,
  rmSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";

const fixtureRoot = fileURLToPath(new URL("../", import.meta.url));
const temporaryRoot = mkdtempSync(
  join(tmpdir(), "static-memory-pool-self-test-"),
);
const binary = join(temporaryRoot, "public-tests");

function run(command, args) {
  const result = spawnSync(command, args, {
    cwd: fixtureRoot,
    stdio: "inherit",
    timeout: 30_000,
  });
  if (result.error) throw result.error;
  if (result.status !== 0) {
    throw new Error(`${command} exited with status ${result.status}`);
  }
}

try {
  run("cc", [
    "-std=c11",
    "-Wall",
    "-Wextra",
    "-Werror",
    "-pedantic",
    "-Istarter",
    "reference/static_memory_pool.c",
    "tests/public/test_static_memory_pool.c",
    "-o",
    binary,
  ]);
  run(binary, []);
  console.log("Static memory-pool trusted reference passed.");
} finally {
  rmSync(temporaryRoot, { recursive: true, force: true });
}

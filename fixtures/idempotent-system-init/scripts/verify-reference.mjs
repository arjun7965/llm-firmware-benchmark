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
  join(tmpdir(), "idempotent-system-init-self-test-"),
);
const binary = join(temporaryRoot, "public-tests");

function run(command, args, options = {}) {
  const result = spawnSync(command, args, {
    cwd: fixtureRoot,
    stdio: "inherit",
    timeout: 30_000,
    ...options,
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
    "-Imocks",
    "reference/idempotent_system_init.c",
    "mocks/mock_idempotent_system_init.c",
    "tests/public/test_idempotent_system_init.c",
    "-o",
    binary,
  ]);
  run(binary, []);
  console.log("Idempotent system-init trusted reference passed.");
} finally {
  rmSync(temporaryRoot, { recursive: true, force: true });
}

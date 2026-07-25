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
  join(tmpdir(), "dual-slot-update-recovery-self-test-"),
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
    throw new Error(command + " exited with status " + result.status);
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
    "reference/dual_slot_update_recovery.c",
    "mocks/mock_dual_slot_update.c",
    "tests/public/test_dual_slot_update_recovery.c",
    "-o",
    binary,
  ]);
  run(binary, []);
  console.log("Dual-slot update recovery trusted reference passed.");
} finally {
  rmSync(temporaryRoot, { recursive: true, force: true });
}

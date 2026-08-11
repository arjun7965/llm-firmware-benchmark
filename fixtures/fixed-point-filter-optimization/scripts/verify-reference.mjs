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
  join(tmpdir(), "fixed-point-filter-optimization-self-test-"),
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
    "-Wvla",
    "-Werror",
    "-pedantic",
    "-Istarter",
    "-Imocks",
    "reference/fixed_point_filter.c",
    "mocks/mock_filter_cost.c",
    "tests/public/test_fixed_point_filter.c",
    "-o",
    binary,
  ]);
  run(binary, []);
  console.log("Fixed-point filter optimization trusted reference passed.");
} finally {
  rmSync(temporaryRoot, { recursive: true, force: true });
}

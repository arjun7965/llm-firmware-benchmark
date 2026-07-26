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
  join(tmpdir(), "resilient-serial-service-self-test-"),
);
const object = join(temporaryRoot, "answer.o");
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
    "-D_GNU_SOURCE",
    "-std=c11",
    "-Wall",
    "-Wextra",
    "-Werror",
    "-pedantic",
    "-Istarter",
    "-Imocks",
    "-include",
    "mocks/redirect_posix.h",
    "-c",
    "reference/resilient_serial_service.c",
    "-o",
    object,
  ]);
  run("cc", [
    "-D_GNU_SOURCE",
    "-std=c11",
    "-Wall",
    "-Wextra",
    "-Werror",
    "-pedantic",
    "-Istarter",
    "-Imocks",
    object,
    "mocks/mock_posix_serial.c",
    "tests/public/test_resilient_serial_service.c",
    "-o",
    binary,
  ]);
  run(binary, []);
  console.log("Resilient serial service trusted reference passed.");
} finally {
  rmSync(temporaryRoot, { recursive: true, force: true });
}
